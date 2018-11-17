
#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>

// TFT display and SD card will share the hardware SPI interface.
// For the Adafruit shield, these are the default.
// The display uses hardware SPI, plus #9 & #10
// Mega2560 Wiring: connect TFT_CLK to 52, TFT_MISO to 50, and TFT_MOSI to 51
#define TFT_DC  9
#define TFT_CS 10
#define SD_CS   6

// touch screen pins, obtained from the documentaion
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin

// width/height of the display when rotated horizontally
#define TFT_WIDTH  320
#define TFT_HEIGHT 240

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

// joystick pins
#define JOY_HORIZ A0
#define JOY_VERT  A1
#define JOY_SEL   2

// neutral position and deadzone of joystick
#define JOY_CENTRE   512
#define JOY_DEADZONE  64

// dimensions of one snake segment/grid box
#define SEG_SIZE 8

// min/max x and y of playable screen (divided into 8x8 grid)
#define GRID_MINX 0
#define GRID_MAXX TFT_WIDTH/SEG_SIZE - 1
#define GRID_MINY 1
#define GRID_MAXY TFT_HEIGHT/SEG_SIZE - 1


Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// all possible game states - always start with menu
enum stateNames {MENU, INS, SETT, INGAME, GAMEOVER};
stateNames state = MENU;

// custom colours for game graphics
uint16_t darkGreen = tft.color565(0, 90, 0);
uint16_t green = tft.color565(80, 255, 50);

// score will be reset every game but highScore will be saved until Arduino is
// reset
short int score = 0;
short int highScore = 0;

// 2d array containing position (x,y) of each snake segment in order
// row index 0 stores snake head position and so on
short int snakeSeg[1160][2] = {0};

// initial snake length (number of grid boxes)
short int length = 4;

// 1 is up, 2 is right, 3 is down, 4 is left
// initially direction of snake movement is right
short int direction = 2;

// coordinates of current displayed item
short int itemX, itemY;

// speed of the snake (number of milliseconds delay between reprinting of
// snake), at medium speed by default
short int speed = 100;

void setup() {
	init();

	tft.begin();
	tft.fillScreen(darkGreen);
	tft.setRotation(3);

	// to ensure random generation of item location, depend on fluctuating analog
	// pin voltage
	randomSeed(analogRead(A5));
}

// every game, reset snake size and direction to 4 segments moving right
void resetSnake() {
	int snakeSeg[1160][2] = {0};
	length = 4;
	direction = 2;
}

// randomly generate x and y coordinates and draw an item at those coordinates
void drawItem() {
	// lower value is inclusive but upper value is exclusive
	itemX = random(GRID_MINX, GRID_MAXX + 1);
	itemY = random(GRID_MINY, GRID_MAXY + 1);

	// draw purple item with white border
	tft.fillRect(itemX*SEG_SIZE, itemY*SEG_SIZE, SEG_SIZE, SEG_SIZE, ILI9341_WHITE);
	tft.fillRect(itemX*SEG_SIZE + 2, itemY*SEG_SIZE + 2, 4, 4, ILI9341_PURPLE);
}

// print menu text
void menuInit() {
	tft.fillScreen(darkGreen);
	tft.setTextColor(green);

	tft.setTextSize(5);
	tft.setCursor(85, 25);
	tft.print("SNAKE");

	tft.setTextSize(2);
	tft.setCursor(136, 100);
	tft.print("PLAY");

	tft.setCursor(88, 145);
	tft.print("INSTRUCTIONS");

	tft.setCursor(112, 190);
	tft.print("SETTINGS");
}

// print game instructions
void insInit() {
	tft.fillScreen(darkGreen);
	tft.setTextColor(green, darkGreen);
	tft.setCursor(0, 0);

	tft.println("Control the direction of");
	tft.println("the snake using the");
	tft.println("joystick. Eating food");
	tft.println("increases your score and");
	tft.println("size. But watch out... if");
	tft.println("you run into the walls or");
	tft.print("yourself, it's game over!");

	tft.setCursor(136, 180);
	tft.print("BACK");
}

// print speed settings text
void settInit() {
	tft.fillScreen(darkGreen);
	tft.setTextColor(green, darkGreen);
	tft.setCursor(52, 25);
	tft.setTextSize(3);

	tft.print("SELECT SPEED");

	tft.setTextSize(2);
	tft.setCursor(112, 100);
	tft.print("SLOWPOKE");

	tft.setCursor(70, 145);
	tft.print("A LITTLE FASTER");

	tft.setCursor(76, 190);
	tft.print("LIKE A FERRARI");
}

// initialize the in-game HUD and screen
void gameInit() {
	tft.fillScreen(darkGreen);
	tft.fillRect(0, 0, TFT_WIDTH, 8, ILI9341_BLACK);
	tft.setTextColor(green);
	tft.setTextSize(1);

	tft.setCursor(0, 0);
	tft.print("SCORE: ");
	tft.print(score);

	tft.setCursor(240, 0);
	tft.print("HI-SCORE: ");
	tft.print(highScore);

	resetSnake();

	// set the initial position of the first four snake segments
	// the "fifth" segment will not be visible and will just overwrite the trail
	// with the background colour
	for (int i = 0; i <= length; i++) {
		snakeSeg[i][0] = 21 - i;
		snakeSeg[i][1] = 15;
	}

	// generate first item
	drawItem();
}

// allow player to use touchscreen input to start the game or read instructions
void menu() {
	TSPoint touch = ts.getPoint();

	// get the y coordinate of where the display was touched
	// remember the x-coordinate of touch is really our y-coordinate
	// on the display
	int touchY = map(touch.x, TS_MINX, TS_MAXX, 0, TFT_HEIGHT - 1);

	// need to invert the x-axis, so reverse the
	// range of the display coordinates
	int touchX = map(touch.y, TS_MINY, TS_MAXY, TFT_WIDTH - 1, 0);

	// if "PLAY" is touched, quickly highlight it and then start game
	if (touchY > 100 && touchY < 120 && touchX > 112 && touchX < 208) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(136, 100);
		tft.print("PLAY");

		delay(500);

		gameInit();

		state = INGAME;
	}
	// if "INSTRUCTIONS" is touched, quickly highlight it then show instructions
	else if (touchY > 145 && touchY < 165 && touchX > 88 && touchX < 232) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(88, 145);
		tft.print("INSTRUCTIONS");

		delay(500);

		insInit();

		state = INS;
	}
	// if "SETTINGS" is touched, quickly highlight it then show settings
	else if (touchY > 190 && touchY < 215 && touchX > 112 && touchX < 208) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(112, 190);
		tft.print("SETTINGS");

		delay(500);

		settInit();

		state = SETT;
	}

}

// while viewing the instructions, the player may use the touchscreen to go
// back to the menu
void ins() {
	TSPoint touch = ts.getPoint();
	int touchY = map(touch.x, TS_MINX, TS_MAXX, 0, TFT_HEIGHT - 1);
	int touchX = map(touch.y, TS_MINY, TS_MAXY, TFT_WIDTH - 1, 0);

	// if "BACK" is touched, quickly highlight it and then go to menu
	if (touchY > 180 && touchY < 196 && touchX > 136 && touchX < 184) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(136, 180);
		tft.print("BACK");

		delay(500);

		menuInit();

		state = MENU;
	}
}

// while in the speed settings, the player can use the touchscreen to select
// from three possible snake speeds
void sett() {
	TSPoint touch = ts.getPoint();
	int touchY = map(touch.x, TS_MINX, TS_MAXX, 0, TFT_HEIGHT - 1);
	int touchX = map(touch.y, TS_MINY, TS_MAXY, TFT_WIDTH - 1, 0);

	// if "SLOWPOKE" is touched, quickly highlight it and then set slowest speed
	if (touchY > 100 && touchY < 120 && touchX > 136 && touchX < 184) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(112, 100);
		tft.print("SLOWPOKE");
		speed = 150;
		delay(500);

		menuInit();

		state = MENU;
	}
	// if "A LITTLE FASTER" is touched, quickly highlight it then set medium speed
	else if (touchY > 145 && touchY < 165 && touchX > 70 && touchX < 250) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(70, 145);
		tft.print("A LITTLE FASTER");
		speed = 100;
		delay(500);

		menuInit();

		state = MENU;
	}
	// if "LIKE A FERRARI" is touched, quickly highlight it then set fastest speed
	else if (touchY > 190 && touchY < 215 && touchX > 76 && touchX < 244) {
		tft.setTextColor(darkGreen, green);
		tft.setCursor(76, 190);
		tft.print("LIKE A FERRARI");
		speed = 50;
		delay(500);

		menuInit();

		state = MENU;
	}
}

// game over screen shows game score and tells the player if a new high score
// was achieved, then automatically starts the menu
void gameOver() {
	snakeSeg[0][0] = constrain(snakeSeg[0][0], GRID_MINX, GRID_MAXX);
	snakeSeg[0][1] = constrain(snakeSeg[0][1], GRID_MINY, GRID_MAXY);

	// snake head turns red when it hits the walls or itself
	tft.fillRect(snakeSeg[0][0]*SEG_SIZE + 2, snakeSeg[0][1]*SEG_SIZE + 2, 4, 4, ILI9341_RED);

	delay(1000);

	tft.fillScreen(darkGreen);

	tft.setTextColor(green);
	tft.setTextSize(5);
	tft.setCursor(25, 48);
	tft.print("GAME OVER");

	tft.setTextColor(green);
	tft.setTextSize(2);
	tft.setCursor(106, 120);
	tft.print("SCORE: ");
	tft.print(score);

	// if a new high score was achieved...
	if (score > highScore) {
		highScore = score;
		tft.setCursor(22, 168);
		tft.print("CONGRATS! NEW HI-SCORE!");
	}

	score = 0;		// reset score to 0 for next game

	delay(5000);

	menuInit();

	state = MENU;
}

// function to redraw body and head of snake
void redrawSnake() {
	// print snake segments at their specified positions
	for (int i = 0; i < length; i++) {
		// drawing black and green snake
		tft.fillRect(snakeSeg[i][0]*SEG_SIZE, snakeSeg[i][1]*SEG_SIZE, SEG_SIZE, SEG_SIZE, green);
		tft.fillRect(snakeSeg[i][0]*SEG_SIZE + 1, snakeSeg[i][1]*SEG_SIZE + 1, 6, 6, ILI9341_BLACK);
	}

	// make the snake head stand out
	tft.fillRect(snakeSeg[0][0]*SEG_SIZE + 2, snakeSeg[0][1]*SEG_SIZE + 2, 4, 4, green);

	// overwrite snake trail with background colour
	tft.fillRect(snakeSeg[length][0]*SEG_SIZE, snakeSeg[length][1]*SEG_SIZE, SEG_SIZE, SEG_SIZE, darkGreen);
}

// function to update body and head position of snake
void updateSnake() {
	// shift position of a segment to the position of the segment before it so the
	// snake body always follows the head exactly
	for (int i = length; i > 0; i--) {
		snakeSeg[i][0] = snakeSeg[i-1][0];
		snakeSeg[i][1] = snakeSeg[i-1][1];
	}

	// change the position of the head based on the current direction
	switch (direction) {
		case 1:
			// if moving up, change y coordinate of head upward
			snakeSeg[0][1] -= 1;
			break;

		case 2:
			// if moving right, change x coordinate of head to the right
			snakeSeg[0][0] += 1;
			break;

		case 3:
			// if moving down, change y coordinate of head downward
			snakeSeg[0][1] += 1;
			break;

		case 4:
			// if moving left, change x coordinate of head to the left
			snakeSeg[0][0] -= 1;
			break;
	}
}

// function to check if the game is lost by snake hitting the wall or itself
// also check if the item has spawned on the snake, if so, print another item
void checkGameLoss() {
	// game ends if the snake tries to go out of bounds of the game screen
	if(snakeSeg[0][0] < GRID_MINX || snakeSeg[0][0] > GRID_MAXX || snakeSeg[0][1] < GRID_MINY || snakeSeg[0][1] > GRID_MAXY) {
		state = GAMEOVER;
	}
	// game also ends if the snake head comes in contact with its body
	else {
		for (int i = 1; i < length; i++) {
			if (snakeSeg[0][0] == snakeSeg[i][0] && snakeSeg[0][1] == snakeSeg[i][1]) {
				snakeSeg[0][0] = snakeSeg[1][0];
				snakeSeg[0][1] = snakeSeg[1][1];
				state = GAMEOVER;
				break;
			}
			// if item tries to spawn in a spot already occupied by a snake segment,
			// generate another one
			else if (snakeSeg[i][0] == itemX && snakeSeg[i][1] == itemY){
				drawItem();
				break;
			}
		}
	}
}

// change snake position based on joystick input
void inGame() {
	int xVal = analogRead(JOY_HORIZ);		// read position of joystick
	int yVal = analogRead(JOY_VERT);

	redrawSnake();

	// 1 is up, 2 is right, 3 is down, 4 is left
  if (xVal > JOY_CENTRE + JOY_DEADZONE && direction != 2 && direction != 4) {
    direction = 4;
  }
  else if (xVal < JOY_CENTRE - JOY_DEADZONE && direction != 4 && direction != 2) {
    direction = 2;
  }
  else if (yVal < JOY_CENTRE - JOY_DEADZONE && direction != 3 && direction != 1) {
    direction = 1;
  }
  else if (yVal > JOY_CENTRE + JOY_DEADZONE && direction != 1 && direction != 3) {
    direction = 3;
  }

	updateSnake();

	// if snake head comes in contact with an item, generate a new item, increase
	// and update the score on the HUD, and increase the length of the snake thus
	// adding a new snake segment
	if (snakeSeg[0][0] == itemX && snakeSeg[0][1] == itemY) {
		drawItem();
		tft.setCursor(42, 0);
		tft.fillRect(42, 0, 18, 8, ILI9341_BLACK);
		score += 1;																		// reprinting score
		tft.print(score);
		length++;
		snakeSeg[length][0] = snakeSeg[length - 1][0];
		snakeSeg[length][1] = snakeSeg[length - 1][1];
	}

	checkGameLoss();
}

int main() {
	setup();

	// initalize menu at start of program
	menuInit();

	// continuously run according to current state
	while (true) {
		if (state == MENU) {
			menu();
		}
		else if (state == INS) {
			ins();
		}
		else if (state == SETT) {
			sett();
		}
		else if (state == INGAME) {
			inGame();
			delay(speed);		// delay between iterations acts as speed of snake
		}
		else if (state == GAMEOVER) {
			gameOver();
		}
	}

	return 0;
}
