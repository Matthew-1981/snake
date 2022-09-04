#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <pthread.h>
#include <ncurses.h>
#include <string.h>

#define MATRIX_MAX_SIZE 100

#ifndef TRUE
#   define TRUE  1
#endif
#ifndef FALSE
#   define FALSE 0
#endif

#define SNAKE_FRUIT -1
#define NO_SNAKE    0
#define SNAKE_LEFT  1
#define SNAKE_RIGHT 2
#define SNAKE_UP    3
#define SNAKE_DOWN  4

#define SNAKE_OK        0
#define SNAKE_COLLISION 1


struct Vector2D {
    ssize_t x;
    ssize_t y;
};

struct SnakeMatrix {
    ssize_t x;
    ssize_t y;
    char matrix[MATRIX_MAX_SIZE][MATRIX_MAX_SIZE];

    struct Vector2D tail;
    struct Vector2D head;
};

typedef struct SnakeMatrix* snake_t;

snake_t SnakeInit (ssize_t x, ssize_t y, size_t snake_lenght, ssize_t head_x, ssize_t head_y)
{
    if (
        x >= MATRIX_MAX_SIZE || y >= MATRIX_MAX_SIZE
        || head_x < 0 || head_x >= x || head_y < 0
        || head_y >= y || head_x - snake_lenght < 0
    )  return NULL;

    snake_t out = malloc( sizeof(struct SnakeMatrix) );
    out->x = x;
    out->y = y;
    out->head = (struct Vector2D){head_x, head_y};
    out->tail = (struct Vector2D){head_x, head_y - snake_lenght + 1};

    for ( size_t i = 0; i < x; i++ ) {
        for ( size_t j = 0; j < y; j++ )
            out->matrix[i][j] = NO_SNAKE;
    }

    for ( size_t i = 0; i < snake_lenght; i++ )
        out->matrix[head_x][head_y - i] = SNAKE_RIGHT;

    return out;
}

int SnakeMove (snake_t snake, int side, int cut_tail)
{
    struct Vector2D* head = &snake->head;
    struct Vector2D* tail = &snake->tail;

    if ( cut_tail ) {
        int tmp = snake->matrix[tail->x][tail->y];
        snake->matrix[tail->x][tail->y] = NO_SNAKE;

        switch ( tmp ) {
            case SNAKE_LEFT:
                tail->y = tail->y - 1;
                break;

            case SNAKE_RIGHT:
                tail->y = tail->y + 1;
                break;

            case SNAKE_UP:
                tail->x = tail->x - 1;
                break;

            case SNAKE_DOWN:
                tail->x = tail->x + 1;
                break;
        }
    }

    int tmp = ( (side == SNAKE_LEFT || side == SNAKE_UP) ? -1 : 1 ) ;

    if ( side == SNAKE_LEFT || side == SNAKE_RIGHT ) {
        if ( head->y + tmp < 0 || head->y + tmp >= snake->y || snake->matrix[head->x][head->y + tmp] > NO_SNAKE )
            return SNAKE_COLLISION;

        snake->matrix[head->x][head->y] = side;
        head->y = head->y + tmp;
        snake->matrix[head->x][head->y] = side;

    } else {
        if ( head->x + tmp < 0 || head->x + tmp >= snake->x || snake->matrix[head->x + tmp][head->y] > NO_SNAKE )
            return SNAKE_COLLISION;

        snake->matrix[head->x][head->y] = side;
        head->x = head->x + tmp;
        snake->matrix[head->x][head->y] = side;
    }

    return SNAKE_OK;
}

#define SnakeDelete(snake) free(snake)

// fruit

int CheckFruit (snake_t snake, int side)
{
    struct Vector2D tmp = snake->head;
    switch ( side ) {
        case SNAKE_RIGHT:
            tmp.y += 1;
            break;

        case SNAKE_LEFT:
            tmp.y -= 1;
            break;

        case SNAKE_UP:
            tmp.x -= 1;
            break;

        case SNAKE_DOWN:
            tmp.x += 1;
            break;
    }

    if ( snake->matrix[tmp.x][tmp.y] == SNAKE_FRUIT )
        return TRUE;

    return FALSE;
}

void GenerateFruit (snake_t snake)
{
    unsigned int x, y;

    for ( size_t i = 0; i < 5; i++ ) {
        x = rand() % snake->x;
        y = rand() % snake->y;

        if ( snake->matrix[x][y] == NO_SNAKE ) {
            snake->matrix[x][y] = SNAKE_FRUIT;
            return;
        }
    }

    size_t safety_check = 0;
    while ( safety_check < snake->x * snake->y && snake->matrix[x][y] != NO_SNAKE ) {
        if ( ++x >= snake->x ) {
            x = 0;
            if ( ++y >= snake->y )
                y = 0;
        }

        safety_check++;
    }

    if ( safety_check != snake->x * snake->y )
        snake->matrix[x][y] = SNAKE_FRUIT;
}

// draw snake

void SnakeDraw (snake_t snake, size_t mv_x, size_t mv_y)
{
    refresh();
    move( mv_x++, mv_y );
    refresh();

    printw( "+" );
    for ( size_t i = 0; i < snake->y; i++ )
        printw( "-" );
    printw( "+" );

    refresh();

    for ( size_t i = 0; i < snake->x; i++ ) {
        move( mv_x++, mv_y );
        refresh();
        printw( "|" );

        for ( size_t j = 0; j < snake->y; j++ ) {
            switch ( snake->matrix[i][j] ) {
                case NO_SNAKE:     printw( " " );  break;
                case SNAKE_FRUIT:  printw( "O" );  break;
                default:           printw( "#" );  break;
            }
        }

        printw( "|" );
    }

    move( mv_x, mv_y );
    refresh();

    printw( "+" );
    for ( size_t i = 0; i < snake->y; i++ )
        printw( "-" );
    printw( "+" );

    refresh();
}

void ScoreDraw (ssize_t score, size_t mv_x, size_t mv_y)
{
    move(mv_x, mv_y);
    refresh();
    printw( "score: %ld", score );
    refresh();
}

void GameOverDraw (size_t mv_x, size_t mv_y)
{
    move(mv_x, mv_y);
    refresh();
    printw( "Game over!" );
    refresh();
}

// game

int button;
pthread_mutex_t button_mutex;

void* ButtonDetect ()
{
    int tmp;
    while (1) {
        tmp = getch();
        pthread_mutex_lock( &button_mutex );
        button = tmp;
        pthread_mutex_unlock( &button_mutex );
    }

    return NULL;
}

int GetDirection (int keypress)
{
    switch ( keypress ) {
        case 's':
        case 'S':
        case KEY_DOWN:
            return SNAKE_DOWN;

        case 'w':
        case 'W':
        case KEY_UP:
            return SNAKE_UP;

        case 'a':
        case 'A':
        case KEY_LEFT:
            return SNAKE_LEFT;

        case 'd':
        case 'D':
        case KEY_RIGHT:
            return SNAKE_RIGHT;
    }

    return SNAKE_RIGHT;
}

void SnakeGame (snake_t snake, int time_cycle, size_t fruit_amount)
{
    pthread_t button_thread;
    int out = SNAKE_OK;
    int move, prev;
    ssize_t score = 0;

    struct winsize w;
    ioctl(0, TIOCGWINSZ, &w);

    const size_t x_mv = ( w.ws_row - snake->x ) / 2 - 1;
    const size_t y_mv = ( w.ws_col - snake->y ) / 2 - 1;

    pthread_mutex_init( &button_mutex, NULL );
    pthread_create( &button_thread, NULL, &ButtonDetect, NULL );

    srand( time(NULL) );
    for ( size_t i = 0; i < fruit_amount; i++ )
        GenerateFruit( snake );

    SnakeDraw(snake, x_mv, y_mv);
    for ( int i = 3; i >= 1; i-- ) {
        move( x_mv - 1, y_mv );
        refresh();
        printw("%d", i);
        refresh();
        sleep(1);
    }

    move( x_mv - 1, y_mv );
    refresh();
    printw(" ");
    refresh();

    while ( out == SNAKE_OK ) {

        SnakeDraw(snake, x_mv, y_mv);
        ScoreDraw(score, x_mv + snake->x + 2, y_mv);
        usleep(time_cycle);

        pthread_mutex_lock( &button_mutex );
        move = button;
        pthread_mutex_unlock( &button_mutex );

        move = GetDirection(move);

        if (
            (prev == SNAKE_LEFT && move == SNAKE_RIGHT)
            || (prev == SNAKE_RIGHT && move == SNAKE_LEFT)
            || (prev == SNAKE_UP && move == SNAKE_DOWN)
            || (prev == SNAKE_DOWN && move == SNAKE_UP)
        )  move = prev;

        prev = move;

        int move_tail = !CheckFruit(snake, move);
        out = SnakeMove( snake, move, move_tail );

        if ( !move_tail ) {
            GenerateFruit( snake );
            score++;
        }
    }

    pthread_kill( button_thread, 0 );
    pthread_mutex_destroy( &button_mutex );

    GameOverDraw( x_mv - 1, y_mv );
    sleep(3);
}

int main (int argc, char* argv[])
{
    ssize_t size_x = 16;
    ssize_t size_y = 16;
    size_t time_interval = 100000;
    size_t fruit_amount = 1;

    for ( int i = 1; i < argc; i++ ) {

        if ( strcmp( argv[i], "-speed" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf( stderr, "error: -speed: use '-speed [1, inf)'\n" );
                return 1;
            }
            int tmp = atoi( argv[i] );
            if ( tmp <= 0 ) {
                fprintf( stderr, "error: -speed: use '-speed [1, inf)'\n" );
                return 1;
            }
            time_interval = tmp * 1000;

        } else if ( strcmp( argv[i], "-fruit" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf( stderr, "error: -fruit: use '-fruit [1, x*y-3]'\n" );
                return 1;
            }
            int tmp = atoi( argv[i] );
            if ( tmp < 1 || tmp >= size_x * size_y - 3 ) {
                fprintf( stderr, "error: -fruit: use '-fruit [1, x*y-3]'\n" );
                return 1;
            }
            fruit_amount = tmp;

        } else if ( strcmp( argv[i], "-size" ) == 0 ) {
            if ( ++i >= argc ) {
                fprintf( stderr, "error: -size: use '-size [8, 99] [8, 99]'\n" );
                return 1;
            }
            int tmp = atoi( argv[i] );
            if ( tmp < 8 || tmp >= MATRIX_MAX_SIZE ) {
                fprintf( stderr, "error: -size: use '-size [8, 99] [8, 99]'\n" );
                return 1;
            }
            size_y = tmp;

            if ( ++i >= argc ) {
                fprintf( stderr, "error: -size: use '-size [8, 99] [8, 99]'\n" );
                return 1;
            }
            tmp = atoi( argv[i] );
            if ( tmp < 8 || tmp >= MATRIX_MAX_SIZE ) {
                fprintf( stderr, "error: -size: use '-size [8, 99] [8, 99]'\n" );
                return 1;
            }
            size_x = tmp;

        } else if ( strcmp( argv[i], "-help" ) == 0 ) {
            fprintf( stderr, "flags: '-size', '-speed', '-fruit', '-help'\n" );
            return 0;

        } else {
            fprintf( stderr, "error: unknown flag '%s'\n", argv[i] );
            return 1;
        }

    }

    initscr();
    keypad( stdscr, TRUE );
    noecho();

    clear();

    snake_t snake = SnakeInit( size_x, size_y, 3, size_x / 2, size_y / 2 );
    SnakeGame( snake, time_interval, fruit_amount );
    SnakeDelete(snake);

    endwin();

    return 0;
}
