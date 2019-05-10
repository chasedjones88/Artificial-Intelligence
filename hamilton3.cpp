#include <iostream>
#include <cstdlib>
#include <ratio>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <cmath>
#include <algorithm>
using namespace std;

#define MAX_MOVES 56
const bool GAME_OVER_OFF=false;
const bool LOCK_COMP=true;
const bool COMPUTERSONLY=false;


bool predicted;
int hisT[3008] = {0};
int killerMoves[30][2] = {0};
uint64_t board[11] = {0ULL};

/**********
  Weights
***********/
#define MAXDEPTH 5
#define pW 1
#define hW 10
#define bW 10
#define kW 50
#define moveW 10
#define mobW 5

#define chors board[0]
#define cking board[1]
#define cbish board[2]
#define cpawn board[3]
#define hhors board[4]
#define hking board[5]
#define hbish board[6]
#define hpawn board[7]
#define cpieces board[8]
#define hpieces board[9]
#define allpieces board[10]

/*********************
    MANIPULATIONS
**********************/

uint16_t getHashFromMove(uint16_t move){
  uint16_t hash = (move & 0xff00) >> 2;
  hash += (move & 0xff);
  return hash;
}
// gets number of pieces on a given board
short getCountFromBoard(uint64_t currboard){
  short count = 0;
  for(int i = 0; i < 48; i++){
    uint64_t mask = 1ULL << i;
    // piece is found
    if(currboard & mask){
      count++;
    }
  }
  return count;
}
// gets column char from given index on the board
void getColFromIndex(short index, string &strRow){
  short row = index%8;
  switch (row) {
    case 0: strRow = "h"; break;
    case 1: strRow = "g"; break;
    case 2: strRow = "f"; break;
    case 3: strRow = "e"; break;
    case 4: strRow = "d"; break;
    case 5: strRow = "c"; break;
    case 6: strRow = "b"; break;
    case 7: strRow = "a"; break;
  }
}
// gets move string from move [index](8 bits)[index](8 bits)
void getMoveString(uint16_t move, string &str){
  uint16_t amask = 0xff00;
  uint16_t bmask = 0x00ff;
  short index = (move & amask) >> 8;

  string moveStr;
  getColFromIndex(index, moveStr);
  moveStr += (char)(index / 8) + 49;
  index = move & bmask;
  string tmp;
  getColFromIndex(index, tmp);
  moveStr += tmp;
  moveStr += (char)(index / 8) + 49;
  str = moveStr;
}
// gets move string from move [index](8 bits)[index](8 bits) for other player to enter
void getReverseMoveString(uint16_t move, string &str){
  uint16_t amask = 0xff00;
  uint16_t bmask = 0x00ff;
  short index = (move & amask) >> 8;

  string moveStr;
  getColFromIndex(index, moveStr);
  moveStr += to_string((long long)abs(6-(index / 8)));
  index = move & bmask;
  string tmp;
  getColFromIndex(index, tmp);
  moveStr += tmp;
  moveStr += to_string((long long)abs(6-(index / 8)));
  str = moveStr;
}
// gets indices from a move
void getLocFromMove(uint64_t masks[], uint16_t move){
  uint16_t amask = 0xff00;
  uint16_t bmask = 0x00ff;

  masks[0] = 1ULL << ((move & amask) >> 8);
  masks[1] = 1ULL << (move & bmask);
}
// takes in string and returns the move in format [index](8 bits)[index](8 bits)
uint16_t getMoveFromString(string str){
  uint16_t move = 0;
  bool valid = true;

  // get column value
  switch(str.at(0)){
    case 'a':move = (uint16_t) 7; break;
    case 'b':move = (uint16_t) 6; break;
    case 'c':move = (uint16_t) 5; break;
    case 'd':move = (uint16_t) 4; break;
    case 'e':move = (uint16_t) 3; break;
    case 'f':move = (uint16_t) 2; break;
    case 'g':move = (uint16_t) 1; break;
    case 'h':move = (uint16_t) 0; break;
    default: valid = false; break;
  }
  // get row value
  move += (uint16_t)(((char)str.at(1) - 49)*8);
  move = move << 8;
  // get column value
  uint16_t target = 0xffff;
  switch(str.at(2)){
    case 'a':target = (uint16_t) 7; break;
    case 'b':target = (uint16_t) 6; break;
    case 'c':target = (uint16_t) 5; break;
    case 'd':target = (uint16_t) 4; break;
    case 'e':target = (uint16_t) 3; break;
    case 'f':target = (uint16_t) 2; break;
    case 'g':target = (uint16_t) 1; break;
    case 'h':target = (uint16_t) 0; break;
    default: valid = false; break;
  }
  // get row value
  target += (uint16_t)(((char)str.at(3) - 49)*8);

  move = move + target;

  if(valid)
    return move;
  else
    return (uint16_t) 0xffff;
}
// takes in two 64 bits masks (from, to) and outputs them as a 16 bit move
uint16_t getMoveFromMask(uint64_t a, uint64_t b){
  // index of a and b on the board
  short a_pos=-1;
  short b_pos=-1;
  for(short i = 0; i < 48; i++){
    if(a_pos == -1){
      if(a == 1ULL)
        a_pos = i;
      else
        a = a >> 1;
    }
    if(b_pos == -1){
      if(b == 1ULL)
        b_pos = i;
      else
        b = b >> 1;
    }
  }
  uint16_t move = a_pos << 8;
  move += b_pos;
  return move;

}

/*********************
    MOVE GENERATION
**********************/

// modifies an array of valid moves for horses
// returns length of horse move array
// returns at most 16 moves
int generateHorseMoves(uint64_t board[], uint16_t hmoves[], bool comp, int active){
  short moveIndex = 0;

  // get correct bishop board
  short hindex = 0;
  if(!comp)
    hindex = 4;

  int i = 47;
  while(active > 0 && i >= 0){
    uint64_t mask = 1ULL << i;
    // piece is found
    if(board[hindex] & mask){
      // check 8 possibilies possible for piece
      for(short j = 0; j < 8; j++){
        short searchindex=0;
        // set which diagonal to check
        switch(j){
          case 0: searchindex = i + 17; break; // move up 2 and left
          case 1: searchindex = i + 15; break; // move up 2 and right
          case 2: searchindex = i + 10; break; // move left 2 and up
          case 3: searchindex = i + 6; break; // move right 2 and up
          case 4: searchindex = i - 6; break; // move left 2 and down
          case 5: searchindex = i - 10; break; // move right 2 and down
          case 6: searchindex = i - 15; break; // move down 2 and left
          case 7: searchindex = i - 17; break; // move down 2 and right
        }
        // if search is invalid, don't bother checking
        if (searchindex < 49 && searchindex > -1){
          // mask off move targets
          uint64_t moveMask = 1ULL << searchindex;

          // moving on these conditions would wrap around the board. ingore these moves
          if (!( (i % 8 == 0 && (j % 2 == 1 )) // if on right hand side, don't allow moves that wrap right
                 || (i % 8 == 1 && (j == 3 || j == 5)) // if in column G, don't allow moves that wrap right
                 || (i % 8 == 6 && (j == 2 || j == 4)) // if in column B, don't allow moves that wrap left
                 || (i % 8 == 7 && (j % 2 == 0)) // if on left hand side, don't allow moves that wrap left
               )) {
            bool open = true;
            bool capturing = false;
            // computer is going, cannot capture backward in upper half
            if(comp && j > 3){
              // check all boards
              // space is occupied, cannot move, test for capture
              if(allpieces & moveMask){
                open = false;
                if(hpieces & moveMask)
                  capturing = true;
              }
            }
            // human is going, cannot capture backward in lower half
            else if(!comp && j < 4){
              // check all boards
              // space is occupied, cannot move, test for capture
              if(allpieces & moveMask){
                open = false;
                if(cpieces & moveMask)
                  capturing = true;
              }
            }
            // computer or human is senior piece
            else{
              open = false;
              if(comp && j < 4){
                // check all boards
                // space is occupied, cannot move, test for capture
                if(allpieces & moveMask && i < 24){
                  if(hpieces & moveMask)
                    capturing = true;
                }
              }
              else if(!comp && j > 3){
                if(allpieces & moveMask && i > 23){
                  if(cpieces & moveMask)
                    capturing = true;
                }
              }
            }
            // add move
            if(open || capturing){
              hmoves[moveIndex] = getMoveFromMask(mask, moveMask);
              moveIndex++;
            }
          }
        }
      }
      active--;
    }
    i--;
  }
  return moveIndex;
}
// modifies an array of valid moves for kings
// returns length of kings move array
// returns at most 2 moves
int generateKingMoves(uint64_t board[], uint16_t kmoves[], bool comp, int active){
  short moveIndex = 0;

  // computer kings can only move in the top row, 6 center squares
  int i = 46;
  int iend = 40;
  if(!comp){
    // human kings can only move in the bottom row, 6 center squares
    i = 6;
    iend = 0;
  }

  // if kings exist in top row and in a space in which it can move
  while(active > 0 && i > iend){
    // create mask for piece
    uint64_t mask = 1ULL << i;

    bool open = true;
    bool capturing = false;
    // computer piece is found, when computer turn
    // or human piece is found, when human turn
    if((cking & mask && comp) || (hking & mask && !comp)){
      uint64_t moveMask = 0ULL;
      if(i % 8 > 3){ // piece is to the left
        moveMask = 1ULL << (i+1);
        // computer piece
        if(comp){
          // space is occupied
          if(allpieces & moveMask){
            open = false;
            // occupied by enemy and can capture
            if(hpieces & moveMask){
              capturing = true;
            }
          }
        }
        else{
          // space is occupied
          if(allpieces & moveMask){
            open = false;
            // occupied by enemy and can capture
            if(cpieces & moveMask){
              capturing = true;
            }
          }
        }
      }
      else{ // piece is to the right
        moveMask = 1ULL << (i-1);
        // computer piece
        if(comp){
          // space is occupied
          if(allpieces & moveMask){
            open = false;
            // occupied by enemy and can capture
            if(hpieces & moveMask){
              capturing = true;
            }
          }
        }
        else{
          // space is occupied
          if(allpieces & moveMask){
            open = false;
            // occupied by enemy and can capture
            if(cpieces & moveMask){
              capturing = true;
            }
          }
        }
      }

      if( open || capturing ){ // add move
        kmoves[moveIndex] = getMoveFromMask(mask, moveMask);
        moveIndex++;
      }
      // check next king
      active--;
    }
    // check next index
    i--;
  }
  return moveIndex;
}
// modifies an array of valid moves for bishops
// returns length of bishop move array
// returns at most 10 moves
int generateBishopMoves(uint64_t board[], uint16_t bmoves[], bool comp, int active){
  short moveIndex = 0;

  // get correct bishop board
  short bindex = 2;
  if(!comp)
    bindex = 6;

  int i = 47;
  while(active > 0 && i >= 0){
    uint64_t mask = 1ULL << i;
    // piece is found
    if(board[bindex] & mask){
      // check 4 diagonals possible for piece
      for(short j = 0; j < 4; j++){
        short searchindex=0;
        short searchprev=0;
        // set which diagonal to check
        switch(j){
          case 0: searchindex = i + 9; break; // move up to the left
          case 1: searchindex = i + 7; break; // move up to the right
          case 2: searchindex = i - 7; break; // move down to the left
          case 3: searchindex = i - 9; break; // move down to the right
        }
        // if search is invalid, don't bother checking
        if (searchindex < 48 && searchindex > -1 &&
                !(i % 8 == 7 && (j == 0 || j == 2)) &&
                !(i % 8 == 0 && (j == 1 || j == 3))){
          bool open = true;
          bool capturing = false;
          uint64_t moveMask = 1ULL << searchindex;

          // If in bottom half of owner's board
          if((comp && (j > 1)) || (!comp && (j < 2))){


            // mask off move targets
            if(moveMask & allpieces){ // move target is blocked
              open = false;
              if((comp && moveMask & hpieces) || (!comp && moveMask & cpieces)){ // valid enemy piece to capture
                capturing = true;
              }

              if(open || capturing){ // add move
                bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
                moveIndex++;
              }
            }
            // move target was open, so now check it's children
            else {
              // add the open move
              bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
              moveIndex++;
              searchprev = searchindex;
              switch(j){
                case 0: searchindex = searchindex + 9; break; // move up to the left
                case 1: searchindex = searchindex + 7; break; // move up to the right
                case 2: searchindex = searchindex - 7; break; // move down to the left
                case 3: searchindex = searchindex - 9; break; // move down to the right
              }
              moveMask = 1ULL << searchindex;

              // search all children along diagonal
              while(searchindex < 48 && searchindex > -1 && searchprev < 49 && searchprev > -1){ //while moving is still valid
                if(!(searchprev % 8 == 7 && (j == 0 || j == 2)) && // make sure move won't wrap board
                   !(searchprev % 8 == 0 && (j == 1 || j == 3))){
                   // found a piece in the way
                   if(allpieces & moveMask){
                     // capture
                     if((comp && hpieces & moveMask) || (!comp && cpieces & moveMask)){ // capture and stop checking this diagonal
                       bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
                       moveIndex++;

                     }
                     // regardless if it was a capture or not, break the while
                     break;
                   }
                   else{ // space was open, so add it as a move
                     bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
                     moveIndex++;
                   }

                   // get next index
                   searchprev = searchindex;
                   switch(j){
                     case 0: searchindex = searchindex + 9; break; // move up to the left
                     case 1: searchindex = searchindex + 7; break; // move up to the right
                     case 2: searchindex = searchindex - 7; break; // move down to the left
                     case 3: searchindex = searchindex - 9; break; // move down to the right
                   }
                   moveMask = 1ULL << searchindex;
                }
                else break;

              } // end checking diagonal children
            } // end checking diagonals
          } // end checking junior pieces

          // check senior pieces
          else { 
			if((comp && i < 24) || (!comp && i > 23)){ // make sure its senior
				// piece was found in adjacent corner
				if(allpieces & moveMask){
				  open = false;
				  // can capture the piece
				  if((comp && moveMask & hpieces && i < 24) || (!comp && moveMask & cpieces && i > 23)){
					capturing = true;
					bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
					moveIndex++;
				  }
				}
				else {
				  // do NOT add the open move
				  searchprev = searchindex;
				  switch(j){
					case 0: searchindex = searchindex + 9; break; // move up to the left
					case 1: searchindex = searchindex + 7; break; // move up to the right
					case 2: searchindex = searchindex - 7; break; // move down to the left
					case 3: searchindex = searchindex - 9; break; // move down to the right
				  }
				  moveMask = 1ULL << searchindex;


				  // search all children along diagonal
				  while(searchindex < 48 && searchindex > -1){ //while moving is still valid
					if(!(searchprev % 8 == 7 && (j == 0 || j == 2)) && // make sure move won't wrap board
					   !(searchprev % 8 == 0 && (j == 1 || j == 3))){
					   // found a piece in the way
					   if(allpieces & moveMask){
						 // capture
						 if((comp && hpieces & moveMask) || (!comp && cpieces & moveMask)){ // capture and stop checking this diagonal
						   bmoves[moveIndex] = getMoveFromMask(mask, moveMask);
						   moveIndex++;
						 }
						 // regardless if it was a capture or not, break the while
						 break;
					   }
					}
					else break;

					// get next index
					searchprev = searchindex;
					switch(j){
					  case 0: searchindex = searchindex + 9; break; // move up to the left
					  case 1: searchindex = searchindex + 7; break; // move up to the right
					  case 2: searchindex = searchindex - 7; break; // move down to the left
					  case 3: searchindex = searchindex - 9; break; // move down to the right
					}
					moveMask = 1ULL << searchindex;
				  } // end checking diagonal children
				}
			  }
		  }

        }
      }
      active--;
    }
    i--;
  }
  return moveIndex;
}
// modifies an array of valid moves for pawns
// returns length of pawn move array
// returns at most 18 moves
int generatePawnMoves(uint64_t board[], uint16_t pmoves[], bool comp, int active){
  short moveIndex = 0;

  // get correct pawn board
  short pindex = 3;
  if(!comp)
    pindex = 7;

  // 47-40 and 7-0 omitted since pawns can
  // never be in these spaces,
  // or move if they have reached them
  int i = 39;
  while(active > 0 && i >= 8){
    uint64_t mask = 1ULL << i;
    // piece is found
    if(board[pindex] & mask){
      // check 3 move locations possible for piece
      for(short j = 1; j > -2; j--){
        // mask off move targets
        uint64_t moveMask;
        if(comp)
          moveMask = 1ULL << (i-8 + j);
        else
          moveMask = 1ULL << (i+8 + j);

        // used for forward move only
        // changed if space ahead is not open
        bool spaceOpen = true;

        // used for diagonals
        // changed if can capture
        bool canCapture = false;

        // k = 0
        // can only move forward if open
        if(j == 0){
          // check all boards
          // piece exists in front of pawn, can't move

          if(allpieces & moveMask)
            spaceOpen = false;
        }

        // checking for capture on diagonals, or if space is empty ahead
        // can only capture enemy pieces
        // cannot move off the board
        else {
          // computer enemy boards
          uint64_t enemyBoard = hpieces;

          // human enemy boards
          if(!comp){
            enemyBoard = cpieces;
          }
          // iterate over enemy boards
          // enemy exists as capture target
          if(enemyBoard & moveMask){
            // if moving will move you off the board, exclude this move
            if(!(((i % 8) == 7 && (j == 1)) ||
               ((i % 8) == 0 && (j == -1)))){ // can capture perfectly fine
              canCapture = true;
            }
          }

        } // end if checking for diagonals

        // evaluate if the piece can move
        // if so, add it to the move array
        if((spaceOpen && j == 0) || (canCapture && j != 0)){
          pmoves[moveIndex] = getMoveFromMask(mask, moveMask);
          moveIndex++;
        }
      } // end looking for moves for active piece
      active--;
    } //end if
    i--;
  }// end while
  return moveIndex;
}

// modifies an array of valid move states (pass by reference)
// returns length of array (int)
// returns at maximum 56 moves
int generateMoves(uint64_t board[], uint16_t moves[], bool comp, short pActive[]){

  // maximum number of moves horses may make in every state
  uint16_t hmoves[16] = {0};
  short hlen = 0;
  // maximum number of moves kings may make in every state
  uint16_t kmoves[2] = {0};
  short klen = 0;
  // maximum number of moves bishops may make in every state
  uint16_t bmoves[20] = {0};
  short blen = 0;
  // maximum number of moves pawns may make in every state
  uint16_t pmoves[18] = {0};
  short plen = 0;

  // generate all moves possible
  short activeIndex = 0;
  if(!comp){
    activeIndex = 4;
  }
  hlen = generateHorseMoves(board, hmoves, comp, pActive[activeIndex]);
  klen = generateKingMoves(board, kmoves, comp, pActive[activeIndex+1]);
  blen = generateBishopMoves(board, bmoves, comp, pActive[activeIndex+2]);
  plen = generatePawnMoves(board, pmoves, comp, pActive[activeIndex+3]);

  // iterate over all moves and fill all slots
  short index = 0;
  // add all horse moves
  if(hlen > 0){
    for(short i = 0; i < hlen; i++){
        moves[index] = hmoves[i];
        index++;
      }
  }
  // add all king moves
  if(klen > 0){
    for(short i = 0; i < klen; i++){
        moves[index] = kmoves[i];
        index++;
      }
  }
  // add all bishop moves
  if(blen > 0){
    for(short i = 0; i < blen; i++){
        moves[index] = bmoves[i];
        index++;
      }
  }
  // add all pawn moves
  if(plen > 0){
    for(short i = 0; i < plen; i++){
        moves[index] = pmoves[i];
        index++;
      }
  }

  return (hlen + klen + blen + plen);
}

bool hasMoves(uint64_t board[], bool comp, short pActive[]){
  uint16_t moves[56];
  short len = generateMoves(board, moves, comp, pActive);
  if(len > 0)
    return true;
  else
    return false;
}

/*********************
        DISPLAY
**********************/

// display the board
void displayBoard(uint64_t board[]){
  // rewrite values as we loop through
  char boardout[48] = {0};

  // loop over every game position
  // 63-48 are omitting since they cannot exist on the board.
  for(int i = 47; i >= 0; i--){
    uint64_t mask = 1ULL << i;
    // loop over every bitboard and replace char if bit is found
    for(short j = 0; j < 8; j++){
      char rpchar = 0;
      // sets char based on what board you're searching
      switch(j){
        case 0: rpchar = 'H'; break;
        case 1: rpchar = 'K'; break;
        case 2: rpchar = 'B'; break;
        case 3: rpchar = 'P'; break;
        case 4: rpchar = 'h'; break;
        case 5: rpchar = 'k'; break;
        case 6: rpchar = 'b'; break;
        case 7: rpchar = 'p'; break;
      }
      // if piece exists at this location
      if(board[j] & mask){
        boardout[i] = rpchar;
      }
      // if nothing exists in the array yet
      else{
        if(boardout[i] == 0)
        boardout[i] = '-';
      }
    }
  }

  // header
  cout << "\n=========================== COMPUTER";
  // print board from left to right, top to bottom
  for(short i = 47; i >= 0; i--){
    // display next line identifier
    if((i+1) % 8 == 0){
      cout << "\n\033[0m";
      cout << i/8 + 1;
      cout << "  ";
    }
    // reset color
    cout << "\033[0m";

    // create checkered pattern
    if((i/8 % 2 && i % 2) || (!(i/8 % 2) && !(i%2))){
      if(boardout[i] == 'P' || boardout[i] == 'H' || boardout[i] == 'K' || boardout[i] == 'B')
        cout << "\033[7;31m ";
      else if(boardout[i] == 'p' || boardout[i] == 'h' || boardout[i] == 'k' || boardout[i] == 'b')
        cout << "\033[7;36m ";
      else
        cout << "\033[7m ";
    }
    else{
      if(boardout[i] == 'P' || boardout[i] == 'H' || boardout[i] == 'K' || boardout[i] == 'B')
        cout << "\033[0;31m ";
      else if(boardout[i] == 'p' || boardout[i] == 'h' || boardout[i] == 'k' || boardout[i] == 'b')
        cout << "\033[0;36m ";
      else
        cout << "\033[0m ";
    }
    cout << boardout[i];
    cout << " ";
  }
  // print footer
  cout << "\033[0m\n";
  cout << "    A  B  C  D  E  F  G  H\n\n";

  cout << endl;
}
// display moves in move array
void displayMoves(uint16_t moves[], short len){
  string move;
  cout << "Here are your available moves: " << endl;

  if(len > 0){
    for(int i=0; i < len; i++){
      getMoveString(moves[i], move);
      cout << move << "\t";
    }
  }
}

void displayAllBoards(uint64_t board[]){
  for(int i = 0; i < 11 ; i++){
    cout << "board #" << i << "\t" <<  setw(12)<<std:: hex << board[i] << endl;
  }
}

/*********************
  BOARD MANIPULATIONS
**********************/

bool isCapture(uint64_t board[], uint16_t move){
  uint64_t masks[2] = {0xffffffffffffffffULL};
  getLocFromMove(masks, move);

  // check if captures a piece
  if((hpieces & masks[0] && cpieces & masks[1]) || (hpieces & masks[1] && cpieces & masks[0])){
    return true;
  }
  else return false;
}
// make a move on the board
short makeMove(uint64_t board[], uint16_t move, bool comp, short pActive[]){
  short captureBoard = 50;
  uint64_t masks[2] = {0xffffffffffffffffULL};
  getLocFromMove(masks, move);

  short enemyIndex = 4;
  short enemyIndexEnd = 8;
  short myIndex = 0;
  short myIndexEnd = 4;
  short enemyAll = 9;
  if(!comp){
    enemyIndex = 0;
    enemyIndexEnd = 4;
    myIndex = 4;
    myIndexEnd = 8;
    enemyAll = 8;
  }

  // check if captures a piece
  if(board[enemyAll] & masks[1]){
    // check all enemy boards for which piece is being captured.
    for(int i = enemyIndex; i < enemyIndexEnd; i++){
      // piece is found, zero out position.
      if(board[i] & masks[1]){
        board[i] = board[i] & ~masks[1];
        captureBoard = i;
        break;
      }
    }
  }
  // check all my boards for which piece is moving and move it
  for(int i = myIndex; i < myIndexEnd; i++){
    if(board[i] & masks[0]){
      board[i] = board[i] & ~masks[0]; // remove the piece
      if((i == 0 || i==4) && (((move & 0xff) % 8) < 4)) // horse becomes bishop
        board[i+2] = board[i+2] | masks[1];
      else if((i==2 || i==6) && (((move & 0xff) % 8) > 3)) // bishop becomes horse
        board[i-2] = board[i-2] | masks[1];
      else
        board[i] = board[i] | masks[1]; // add it back in the target
      break;
    }
  }

  // update active counts
  for(int i = 0; i < 8; i++){
    pActive[i] = getCountFromBoard(board[i]);
  }

  cpieces = chors+cking+cbish+cpawn;
  hpieces = hhors+hking+hbish+hpawn;
  allpieces = cpieces+hpieces;
  pActive[8] = pActive[0] + pActive[1] + pActive[2] + pActive[3];
  pActive[9] = pActive[4] + pActive[5] + pActive[6] + pActive[7];
  pActive[10] = pActive[8] + pActive[9];

  return captureBoard;
}
// undo a move and put piece back if it was a capture
void retractMove(uint64_t board[], uint16_t move, bool comp, short pActive[], short captureBoard){
  uint64_t masks[2] = {0xffffffffffffffffULL}; // masks[0] is from, masks[1] is target
  getLocFromMove(masks, move);

  short myIndex = 0;
  short myIndexEnd = 4;
  if(!comp){
    myIndex = 4;
    myIndexEnd = 8;
  }
  string movestr;
  getMoveString(move, movestr);
  // check all my boards for which piece is moving and move it
  for(int i = myIndex; i < myIndexEnd; i++){
    if(board[i] & masks[1]){
      short originindex = (move & 0xff00) >> 8;

      board[i] = board[i] & ~masks[1]; // remove the piece
      if((i == 2 || i == 6) && (( originindex % 8) > 3 )) { // was a horse moving to other side and became a bishop
        board[i-2] = board[i-2] | masks[0];
      }
      else if ((i == 0 || i == 4) && (( originindex % 8) < 4 )) { // was a bishop moving to other side and became a horses
        board[i+2] = board[i+2] | masks[0];
      }
      else{
        board[i] = board[i] | masks[0]; // add it back in the origin
      }
    }
  }
  if(captureBoard != 50){
    board[captureBoard] = board[captureBoard] | masks[1];
  }
  // update active counts
  for(int i = 0; i < 8; i++){
    pActive[i] = getCountFromBoard(board[i]);
  }

  cpieces = chors+cking+cbish+cpawn;
  hpieces = hhors+hking+hbish+hpawn;
  allpieces = cpieces+hpieces;
  pActive[8] = pActive[0] + pActive[1] + pActive[2] + pActive[3];
  pActive[9] = pActive[4] + pActive[5] + pActive[6] + pActive[7];
  pActive[10] = pActive[8] + pActive[9];
}
// set up the board
void setup(uint64_t board[]){
  // default values;/*
  chors = 0x0000C00000000000ULL;
  cking = 0x0000180000000000ULL;
  cbish = 0x0000030000000000ULL;
  cpawn = 0x0000007E00000000ULL;
  hpawn = 0x0000000000007E00ULL;
  hhors = 0x00000000000000C0ULL;
  hking = 0x0000000000000018ULL;
  hbish = 0x0000000000000003ULL;

  cpieces = chors+cking+cbish+cpawn;
  hpieces = hhors+hking+hbish+hpawn;
  allpieces = cpieces+hpieces;

  displayBoard(board);
}

/*********************
      HUERISTICS
**********************/

int evalend(uint64_t board[], short pActive[], short depth, std::chrono::high_resolution_clock::time_point t1, bool comp, int hmoves, int cmoves){
  std::chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
  chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
  // approaching limit
  if(time_span > (chrono::duration<double>) 4.99){
    return -65355;
  }
  int sum = 0;

  // no computer kings remain
  if(cking == 0ULL){
    sum = -5000+depth;
  }
  // no human kings remain
  else if(hking == 0ULL){
    sum = 5000-depth;
  }
  else if(cmoves == 0){
    sum = -5000+depth;
  }
  else {
    sum = 5000-depth;
  }

  if((depth % 2 == 0 && comp) || (depth % 2 == 1 && !comp)) return sum;
  else return -sum;

}

int eval(uint64_t board[], short pActive[], short depth, std::chrono::high_resolution_clock::time_point t1, bool comp, int hmoves, int cmoves){
  std::chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
  chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
  // approaching limit
  if(time_span > (chrono::duration<double>) 4.99){
    return -65355;
  }

  int ckingSum = (pActive[1] == 2) ? 300 : 10;
  int hkingSum = (pActive[5] == 2) ? -300 : 10;

  int sum = hW*pActive[0] + bW*pActive[2] + pW*pActive[3] /* Material */
            - (hW*pActive[4] + bW*pActive[6] + pW*pActive[7])
            + (cmoves*moveW - hmoves*moveW) /* Number of Moves left per person */
            + ckingSum + hkingSum /* weighted sum of kings remaining. If two remain, 300, else 10 */;
			
  /*for(int i = 0; i < 48; i++){
	  if((1ULL << i) & cpieces){
		  sum += (i / 6 + 1)*mobW;
	  }
	  else if ((1ULL << i) & hpieces){
		  sum -= (6 - (i / 6 + 1))*mobW;
	  }
  }*/
			
  if((depth % 2 == 0 && comp) || (depth % 2 == 1 && !comp)) return sum;
  else return -sum;

}

void merge(uint64_t board[], short pActive[], uint16_t moves[][56], short begin, short len, short depth){
  uint16_t sorted[len];
  short sindex = 0;
  short aindex = 0;
  short bindex = len/2;
  while(aindex < len/2 && bindex < len){
    if(isCapture(board, moves[depth][aindex]) && !isCapture(board, moves[depth][bindex]))
      sorted[sindex++] = moves[depth][aindex++];
    else if(!isCapture(board, moves[depth][aindex]) && isCapture(board, moves[depth][bindex]))
      sorted[sindex++] = moves[depth][bindex++];
    else{
      sorted[sindex++] = moves[depth][aindex++];
      sorted[sindex++] = moves[depth][bindex++];
    }
  }

  while(aindex < len/2){
    sorted[sindex++] = moves[depth][aindex++];
  }
  while(bindex < len){
    sorted[sindex++] = moves[depth][bindex++];
  }

  for(int i = begin; i < len; i++){
    moves[depth][i] = sorted[i];
  }
}

void mergesort(uint64_t board[], short pActive[], uint16_t moves[][56], short begin, short len, short depth){
  if(len <= 1) return;

  // mergesort
  mergesort(board, pActive, moves, begin, len/2 , depth);
  mergesort(board, pActive, moves, len/2, len-len/2, depth);

  merge(board, pActive, moves, begin, len, depth);
}
bool mostValuableCapture(uint16_t a, uint16_t b){
	uint64_t masksA[2];
	uint64_t masksB[2];
	getLocFromMove(masksA, a);
	getLocFromMove(masksB, b);
	short boardA = -1;
	short boardB = -1;
	short valueA;
	short valueB;
	for(int i = 0; i < 8; i++){
		if(masksA[1] & board[i] && boardA !=-1)
			boardA = i;
		if(masksB[1] & board[i] && boardB !=-1)
			boardB = i;
	}
	switch(boardA){
		case 0: valueA = 3; break;
		case 4: valueA = 3; break;
		case 1: valueA = 10; break;
		case 5: valueA = 10; break;
		case 2: valueA = 3; break;
		case 6: valueA = 3; break;
		case 3: valueA = 1; break;
		case 7: valueA = 1; break;
	}
	switch(boardB){
		case 0: valueB = 3; break;
		case 4: valueB = 3; break;
		case 1: valueB = 10; break;
		case 5: valueB = 10; break;
		case 2: valueB = 3; break;
		case 6: valueB = 3; break;
		case 3: valueB = 1; break;
		case 7: valueB = 1; break;
	}
	return boardA > valueB;
}
bool historyCheck(uint16_t a, uint16_t b){
  return hisT[getHashFromMove(a)] > hisT[getHashFromMove(b)];
}
bool captureCheck(uint16_t a, uint16_t b){
  return (isCapture(board, a) && !isCapture(board, b));
}
short orderMoves(uint64_t board[], short pActive[], uint16_t moves[][56], short len, short depth, bool comp){
  short captureIndex =0;
  std::sort(moves[depth], moves[depth] + len, captureCheck);
  // get index for noncaptures
  for(; captureIndex < len ;){
	  if(!isCapture(board, moves[depth][captureIndex++]))
		  break;
  }
  std::sort(moves[depth], moves[depth] + captureIndex, mostValuableCapture);
  
  if(depth < 8)
    std::sort(moves[depth]+captureIndex, moves[depth] + len, historyCheck);

  return captureIndex;
}

bool gameOver(uint64_t board[], short pActive[], int *hmoves, int *cmoves, bool comp){
  // no computer kings remain
  if(pActive[1] == 0){
    return true;
  }
  // no human kings remain
  else if(pActive[5] == 0){
    return true;
  }

  // check to see if either side can move, if so game is not over.
  uint16_t moves[MAX_MOVES];
  *hmoves = generateMoves(board, moves, false, pActive);
  *cmoves = generateMoves(board, moves, true, pActive);

  
	if(*cmoves == 0 && comp){
	  return true;
	}
	else if(*hmoves == 0 && !comp){
	  return true;
	}
	else return false;
}

/*********************
     PLAYER TURNS
**********************/
uint16_t minimax(uint64_t board[], short pActive[], std::chrono::high_resolution_clock::time_point t1, bool comp);
int min(uint64_t board[], uint16_t allmoves[][56], short pActive[], short maxdepth, short depth, unsigned int *leafnodes, unsigned int *nonroot, std::chrono::high_resolution_clock::time_point t1, int alpha, int beta, bool comp); // declaration for MAX function

uint16_t getUserMove(uint16_t moves[], short len){

  bool answered = false;
  while (!answered){
    uint16_t move = (uint16_t)0xffff;
    std::string input;
    getline(cin, input);
    std::cin.clear();

    // if answered
    if(!input.empty() && input.length() == 4){
      answered = true;
      // change to bit format [index](8bits)[index](8bits)
      move = getMoveFromString(input);

      bool valid = false;
      for(int i=0; i < len; i++){
        // move was valid was valid
        if(move == moves[i]){
          valid = true;
        }
      }
      if(valid)
        return move;
      else{
        answered = false;
        cout << "Move was invalid. Please make a valid move from above." << endl << ">";
      }
    }
    else
      cout << "Please input a move from above."<< endl << ">";
  }
  return false;
}

void userTurn(uint64_t board[], short pActive[], uint16_t movesPlayed[], int moveNumber){
  cout << "#########################################################################\n";
  cout << "                             User turn!\n";
  cout << "#########################################################################\n\n";

  if(!COMPUTERSONLY){
    uint16_t moves[MAX_MOVES];
    short len = generateMoves(board, moves, false, pActive);
    displayMoves(moves, len);

    cout << "\n\nEnter move: ";

    uint16_t move = getUserMove(moves, len);
    makeMove(board, move, false, pActive);
    movesPlayed[moveNumber] = move;
  }
  else {
    // check time
    std::chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

    uint16_t move = minimax(board, pActive, t1, false); //moves[rand() % len];

    /* check time */
    std::chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
    chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
    cout << "Generating moves took " << time_span.count() <<"s."<< endl;

    makeMove(board, move, false, pActive);
    string movestring;
    string revmovestring;
    movesPlayed[moveNumber] = move;
    getMoveString(move, movestring);
    getReverseMoveString(move, revmovestring);

    cout << "\n\nComputer played move: " << movestring << " (" << revmovestring << ")" << endl;
  }
}

int max(uint64_t board[], uint16_t allmoves[][56], short pActive[], short maxdepth, short depth, unsigned int *leafnodes, unsigned int *nonroot, std::chrono::high_resolution_clock::time_point t1, int alpha, int beta, bool comp)
{
  int cmoves, hmoves = 0;
  if(gameOver(board, pActive, &hmoves, &cmoves, comp)){
    *leafnodes+=1;
    return evalend(board, pActive, depth, t1, true, hmoves, cmoves);
  }
  else if(maxdepth == depth) {
    *leafnodes+=1;
    return eval(board, pActive, depth, t1, true, hmoves, cmoves);
  }
  else{
    int myalpha = alpha;
    int mybeta = beta;
    // computer turn
    int bestScore = -9999;
    short len = generateMoves(board, allmoves[depth], comp, pActive);
    *nonroot += len;
    short noncapture = orderMoves(board, pActive, allmoves, len, depth, true);
	
	for(int i = 0; i < noncapture; i++){
      // retract variables
      short captureBoard;

      // make the move
      captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
      // get new score and compare to previous best
      int score = min(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
      retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

      if( score == -65355)
        return -65355;
      else bestScore = max(bestScore, score);

      myalpha = max(myalpha, bestScore);
      if(mybeta <= myalpha){
        *leafnodes += len-i;
        hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
        killerMoves[depth][1] = killerMoves[depth][0];
        killerMoves[depth][0] = allmoves[depth][i];
        break;
      }
    }
	
    for(int i = noncapture; i < len; i++){
      if(allmoves[depth][i] == killerMoves[depth][0] || allmoves[depth][i] == killerMoves[depth][1]){
        // retract variables
        short captureBoard;

        // make the move
        captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
        // get new score and compare to previous best
        int score = min(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
        retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

        if( score == -65355)
          return -65355;
        else bestScore = max(bestScore, score);

        myalpha = max(myalpha, bestScore);
        if(mybeta <= myalpha){
		  hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
          return bestScore;
        }
      }
    }

    for(int i = noncapture; i < len; i++){
      // retract variables
      short captureBoard;

      // make the move
      captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
      // get new score and compare to previous best
      int score = min(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
      retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

      if( score == -65355)
        return -65355;
      else bestScore = max(bestScore, score);

      myalpha = max(myalpha, bestScore);
      if(mybeta <= myalpha){
        *leafnodes += len-i;
        hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
        killerMoves[depth][1] = killerMoves[depth][0];
        killerMoves[depth][0] = allmoves[depth][i];
        break;
      }
    }
  return bestScore;
  }
}

int min(uint64_t board[], uint16_t allmoves[][56], short pActive[], short maxdepth, short depth, unsigned int *leafnodes, unsigned int *nonroot, std::chrono::high_resolution_clock::time_point t1, int alpha, int beta, bool comp)
{
  int cmoves, hmoves = 0;
  if(gameOver(board, pActive, &hmoves, &cmoves, comp)){
    *leafnodes+=1;
    return evalend(board, pActive, depth, t1, comp, hmoves, cmoves);
  }
  else if(maxdepth == depth) {
    *leafnodes+=1;
    return eval(board, pActive, depth, t1, comp, hmoves, cmoves);
  }
  else{
    int myalpha = alpha;
    int mybeta = beta;
    // human turn
    int bestScore = 9999;
    short len = generateMoves(board, allmoves[depth], comp, pActive);
    *nonroot += len;
    short noncapture = orderMoves(board, pActive, allmoves,len, depth, comp);
	
	for(int i = 0; i < noncapture; i++){
      // retract variables
      short captureBoard = 0;

      // make the move
      captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
      // get new score and compare to previous best
      int score = max(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
      retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

      if(score == -65355)
        return -65355;
      else bestScore = min(bestScore, score);

      mybeta = min(mybeta, bestScore);
      if(mybeta <= myalpha){
        *leafnodes += len-i;
        hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
        killerMoves[depth][1] = killerMoves[depth][0];
		killerMoves[depth][0] = allmoves[depth][i];
        break;
      }
    }
	
	for(int i = noncapture; i < len; i++){
      if(allmoves[depth][i] == killerMoves[depth][0] || allmoves[depth][i] == killerMoves[depth][1]){
        // retract variables
        short captureBoard;

        // make the move
        captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
        // get new score and compare to previous best
        int score = max(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
        retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

        if( score == -65355)
          return -65355;
        else bestScore = min(bestScore, score);

        mybeta = min(mybeta, bestScore);
		if(mybeta <= myalpha){
          hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
          return bestScore;
        }
      }
    }
	
    for(int i = noncapture; i < len; i++){
      // retract variables
      short captureBoard = 0;

      // make the move
      captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
      // get new score and compare to previous best
      int score = max(board, allmoves, pActive, maxdepth, depth+1, leafnodes, nonroot, t1, myalpha, mybeta, !comp);
      retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

      if(score == -65355)
        return -65355;
      else bestScore = min(bestScore, score);

      mybeta = min(mybeta, bestScore);
      if(mybeta <= myalpha){
        *leafnodes += len-i;
        hisT[getHashFromMove(allmoves[depth][i])] += std::pow(2,depth);
        killerMoves[depth][1] = killerMoves[depth][0];
		killerMoves[depth][0] = allmoves[depth][i];
        break;
      }
    }
    return bestScore;
  }
}

uint16_t minimax(uint64_t board[], short pActive[], std::chrono::high_resolution_clock::time_point t1, bool comp){
  predicted = false;
  // actual best move "played" so far
  uint16_t bestMoveSoFar = 0;
  // used for depth test
  short depth = 0;
  int bestScore = -9999;
  unsigned int leafnodes = 0;
  unsigned int nonroot = 0;

  // all possible moves that can be generated to save memory
  uint16_t allmoves[30][56];

  // used for IDS
  // bool timeout;

  short len = generateMoves(board, allmoves[depth], comp, pActive);
  int score;
  orderMoves(board, pActive, allmoves, len, depth, comp);
  displayMoves(allmoves[depth], len);
  nonroot +=len;
  for(int k = 1; k < 30 ; k++){
    int alpha = -9999;
    int beta = 9999;
    cout << "Thinking for depth " << k << "...";
    uint16_t bestmovethisdepth = 0;
    int bestScorethisdepth = -9999;
    for(int i = 0; i < len; i++){
      // retract variables
      short captureBoard = 0;

      // make the move
      captureBoard = makeMove(board, allmoves[depth][i], comp, pActive);
      // get new score and compare to previous best
      score = min(board, allmoves, pActive, k, depth+1, &leafnodes, &nonroot, t1, alpha, beta, !comp);
      retractMove(board, allmoves[depth][i], comp, pActive, captureBoard);

      if(score == -65355){
        break;
      }
      else if( score > bestScorethisdepth){
        bestScorethisdepth = score;
        alpha = score;
        bestmovethisdepth = allmoves[depth][i];
      }
    } // end searching at this depth
    if(score == -65355)
      break;

    bestScore = bestScorethisdepth;
    bestMoveSoFar = bestmovethisdepth;
    cout << "   Best score was " << bestScorethisdepth << endl;
    if(bestScore > 4900 && !predicted){
      cout << "Predicting win in " << k << " moves." << endl;
		predicted = true;
		break;
	}
	else if(bestScore < -4900){
		cout << "Predicting loss in " << k << " moves. But I'm still checking!" << endl;
		predicted = true;
	}
  }

  for(int i = 0; i < 3008; i++){
	  hisT[i] = 0;
  }

  cout << "Total Nodes Visited: " << std::dec << nonroot+1 << endl;
  cout << "Leaf nodes visited: "<< std::dec << leafnodes << endl;
  cout << "Avg Branching Factor: " << (double) (nonroot) / (nonroot - leafnodes +1) << endl;
  return bestMoveSoFar;
}

void computerTurn(uint64_t board[], short pActive[], uint16_t movesPlayed[], int moveNumber){
  cout << "=========================================================================\n";
  cout << "                           Computer turn!\n";
  cout << "=========================================================================\n\n";

  // check time
  std::chrono::high_resolution_clock::time_point t1 = chrono::high_resolution_clock::now();

  /*uint16_t moves[56];
  short len = generateMoves(board, moves, true, pActive);*/
  uint16_t move = minimax(board, pActive, t1, true); //moves[rand() % len];

  /* check time */
  std::chrono::high_resolution_clock::time_point t2 = chrono::high_resolution_clock::now();
  chrono::duration<double> time_span = chrono::duration_cast<chrono::duration<double>>(t2 - t1);
  cout << "Generating moves took " << time_span.count() <<"s."<< endl;

  makeMove(board, move, true, pActive);
  string movestring;
  string revmovestring;
  movesPlayed[moveNumber] = move;
  getMoveString(move, movestring);
  getReverseMoveString(move, revmovestring);

  cout << "\n\nComputer played move: " << movestring << " (" << revmovestring << ")" << endl;
}

/*********************
      GAME FLOW
**********************/

int main()
{
  /*  bitboards
   *  #defined at beginning of file as
   *  board[0] = c. horses
   *  board[1] = c. kings
   *  board[2] = c. bishops
   *  board[3] = c. pawns
   *  board[4] = h. horses
   *  board[5] = h. kings
   *  board[6] = h. bishops
   *  board[7] = h. pawns
   *  board[8] = all computer pieces
   *  board[9] = all human pieces
   *  board[10] = all pieces
   */
  short pActive[] = {2, 2, 2, 6, 2, 2, 2, 6, 12, 12, 24};

  // loop variables for input and deciding who goes first
  bool answered = false;
  bool goingfirst = false;

  uint16_t movesPlayed[120] = {};
  int moveNumber = 0;

  // loop while not answered correctly
  while (!answered){
    cout << "would you like to go first? (y/n)\n";
    std::string input;
    getline(cin, input);
    std::cin.clear();

    if(!input.empty() && (input.at(0) == 'Y'
          || input.at(0) == 'y' || input.at(0) == 'N'
          || input.at(0) == 'n')){
      answered = true;
      if(input.at(0) == 'Y' || input.at(0) == 'y'){
        // human going first
        goingfirst = true;
      } // else implied, computer going first
    }
    else
      // invalid response
      cout << "Please answer 'y' or 'n'\n\n";
  }
  // setup the board
  setup(board);
  int score;

  if(goingfirst==true){
    userTurn(board, pActive, movesPlayed, moveNumber);
    displayBoard(board);
    moveNumber++;
  }

  int hmoves, cmoves;
  // if game isn't over, play the next round
  while(gameOver(board, pActive, &hmoves, &cmoves, false) == false){
    // Computer goes
    computerTurn(board, pActive, movesPlayed, moveNumber);
    moveNumber++;

    // update the display
    displayBoard(board);

    // if game isn't over, user goes
    if(gameOver(board, pActive, &hmoves, &cmoves, true) == false){
      // user goes
      userTurn(board, pActive, movesPlayed, moveNumber);
      moveNumber++;

      // update the display
      displayBoard(board);
    }
	else break;
  }
  score = evalend(board, pActive, 0, chrono::high_resolution_clock::now(), true, hmoves, cmoves);
  if(score > 0){
    cout << "\n\n@@@@@@@@@@@ Computer (your program) won!! @@@@@@@@@@@\n\n";
  }
  else
    cout << "\n\n@@@@@@@@@@@ You (other program) won!! @@@@@@@@@@@\n\n";

  cout << "Moves played were: " << endl;
  bool usermove = goingfirst;
  for(int i = 0; i < moveNumber; i++){
    string move;
    getMoveString(movesPlayed[i], move);
    if(usermove)
      cout << "\033[36m" << move << " ";
    else
      cout << "\033[31m" << move << " ";
    usermove = !usermove;
  }
  cout << "\033[0m]" << endl << endl;

  return 0;
}
