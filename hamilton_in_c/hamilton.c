#include <stdio.h>
#include <string.h>
#include <math.h>

enum Boolean{FALSE=0, TRUE=1};
enum Side{LEFT=0, RIGHT=1};
enum Direction{ DOWN=0, UP=1};
enum Type{PAWN=0, HORSE=1, BISHOP=2, KING=3};
enum Weight{PWN=1, HRS=3, BSHP=3, KNG=7};
enum Infinity{POSMAX=999, NEGMIN=-999};

// set up the board
void setup(char boardstate[]){
  strcpy(boardstate, "HH-KK-BB-PPPPPP------------------pppppp-hh-kk-bb");
}

// reads input for 20 chars or up until \n char
void readInput(char input[]){
  fgets(input, 20, stdin);
}

void displayBoard(char input[]){
  printf("  ================ COMPUTER\n");
  int i = 6;
  for (i > 0; i--) {
    char line[18];
    line[0] = i+48;
    line[1] = " ";
    int row, col;
    row = abs(6-i);
    for (col < 8; col++){

    }
  }
}

int gameOver(char input[], char *userWon){
  return FALSE;
}

void computerTurn(char boardstate[]){
  char str[] = "computer going!";
  printf("%s\n\n", str);
}

void userTurn(char boardstate[], char input[]){
  printf("User going!\n\n");
  readInput(input);
}

int main()
{
  // input buffer
  char input[20];

  // board
  char boardstate[48];

  // boolean loop variables
  char answered = FALSE;
  char goingfirst = FALSE;
  char userWon = FALSE;

  while (answered == FALSE){
    //prompt user
    printf("Would you like to go first? (y/n)\n"); ;
    readInput(input);

    // if user answered and is a valid answer
    if(strlen(input) > 0 && (input[0] == 'Y'
          || input[0] == 'y' || input[0] == 'N'
          || input[0] == 'n')){
      answered = TRUE;
      if(input[0] == 'Y' || input[0] == 'y')
        goingfirst = TRUE;
    }
    else // not a valid response
      printf("Please answer 'y' or 'n'\n\n");
  }

  // setup the board
  setup(boardstate);

  if(goingfirst==TRUE){
    //player goes first
  }

  // if game isn't over, play the next round
  while(gameOver(boardstate, &userWon) == FALSE){
    // Computer goes
    computerTurn(boardstate);

    // update the display
    displayBoard(boardstate);

    // if game isn't over, user goes
    if(gameOver(boardstate, &userWon) == FALSE){
      // user goes
      userTurn(boardstate, input);

      // update the display
      displayBoard(boardstate);
    }
  }

  return 0;
}
