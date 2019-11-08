#ifdef F_CPU
#undef F_CPU
#endif
#define F_CPU 4000000 // 4 MHz

#include "Arduino.h"

#define PIN_A 0
#define PIN_B 2
#define PIN_C 3
#define PIN_D 1
#define PIN_EN 10

#define PIN_RAND 4

#define PIN_SWITCH 6	// AKA MOSI

class Button {
public:
	Button(int pin) : pin(pin) {
	}

	bool pressed();
	bool clicked();
	bool doubleClicked();

protected:
	virtual byte getPinValue();

	int lastButtonValue = 0;
	unsigned long closedTime = 0;

	// Used for clicked()
	bool wasPressed = false;

	// Used for doubleClicked();
	bool wasPressedD = false;
	unsigned long clickedTime = 0;
	bool wasClicked = false;

private:
	int pin;
};

byte Button::getPinValue() {
	return digitalRead(pin);
}

bool Button::pressed() {
	int buttonValue = !getPinValue();	// Goes low when closed

	unsigned long now = millis();

	if (!(now - closedTime > 50)) {
		// Still in blackout period
		return true;
	} else if (buttonValue) {
		closedTime = now;

		return true;
	} else {
		return false;
	}
}

bool Button::clicked() {
	bool nowPressed = pressed();

	if (!wasPressed && nowPressed) {
		wasPressed = true;
		return true;
	}

	wasPressed = nowPressed;

	return false;
}

bool Button::doubleClicked() {
	bool nowPressed = pressed();

	unsigned long now = millis();

	if (!wasPressedD && nowPressed) {
		wasPressedD = true;

		if (wasClicked) {
			// This is a double-click, reset state variables and return true
			wasClicked = false;
			wasPressed = true;	// So that clicked() is reset
			return true;
		}

		// This is the first click
		wasClicked = true;
		clickedTime = now;
		return false;
	}

	if (wasClicked && (now - clickedTime > 325)) {
		// Time the first click out
		wasClicked = false;
	}

	wasPressedD = nowPressed;

	return false;
}

Button button(PIN_SWITCH);

struct SoftPWM {
	byte quantum = 1;
	byte onPercent = 100;
	int count = 0;

	SoftPWM(byte onPercent) {
		reset(onPercent);
	}

	SoftPWM(byte onPercent, byte quantum) {
		this->quantum = quantum;
		reset(onPercent);
	}

	bool off() {
		count = (count + quantum) % 100;
		return count >= onPercent;
	}

	void reset(byte onPercent) {
		this->onPercent = onPercent;
		count = 0;
	}
};

class Display {
protected:
	static unsigned long nowMs;
	static unsigned long lastCalltick;
	static int repCount;

	static unsigned int digit2Bits[];
	static int d;

	static unsigned int interval;
	static int maxReps;

public:
	virtual void init() = 0;
	virtual void display() = 0;
	virtual bool show() {
		nowMs = millis();

		return nowMs - lastCalltick >= interval;
	}
	virtual bool done() {
		repCount++;
		return (repCount >= maxReps);
	}

	static bool displayOn;

protected:
	static void displayDigit() {
		lastCalltick = nowMs;

		unsigned int bits = digit2Bits[abs(d)];
		if (!displayOn) {
			bits = digit2Bits[10];
		}
		digitalWrite(PIN_A, bits & 0x1);
		digitalWrite(PIN_B, bits >> 1 & 0x1);
		digitalWrite(PIN_C, bits >> 2 & 0x1);
		digitalWrite(PIN_D, bits >> 3 & 0x1);
	}
};

unsigned long Display::nowMs = 0;
unsigned long Display::lastCalltick = 0;
int Display::repCount = 0;

// Eleven positions. Last one (index 10) is for blanking
unsigned int Display::digit2Bits[] = { 8, 0, 1, 5, 4, 6, 7, 3, 2, 9, 10 };
int Display::d = 0;
bool Display::displayOn = false;

unsigned int Display::interval = 1000;
int Display::maxReps = 10;

class Riffle: public Display {
protected:
	static unsigned int order2digit[];
	int order = 0;

public:
	virtual void init() {
		repCount = 0;
		interval = 10;
		maxReps = 18 * 2;
		order = 0;
		d = order2digit[order];
	}

	virtual void display() {
		displayDigit();
		order = (order + 10) % 18 - 9;
		d = order2digit[abs(order)];
	}
};

class SlowDown: public Riffle {
public:
	virtual void init() {
		Riffle::init();
		interval = 5;
	}

	virtual void display() {
		Riffle::display();
		interval += 3;
	}
};

// The digits appear in this order in the tube
unsigned int Riffle::order2digit[] = { 1, 0, 2, 9, 3, 8, 4, 7, 5, 6 };

class RandomDisplay: public Display {
public:
	virtual void init() {
		repCount = 0;
		interval = 250;
		randomSeed(analogRead(PIN_RAND));
		maxReps = random(10) + 40;
	}

	virtual void display() {
		d = random(10);
		displayDigit();
	}
};

class Count: public Display {
public:
	virtual void init() {
		repCount = 0;
		interval = 500;
		maxReps = 10;
		d = 0;
	}

	virtual void display() {
		displayDigit();
		d = (d + 1) % 10;
	}
};

class FadeOut : public Display {
	SoftPWM fadePWM;
	int val;
	unsigned long startFade;
	unsigned long start;
	const unsigned long ON_TIME = 600;
	const unsigned long FADE_TIME = 400;
	const unsigned long INTERVAL = 5;
	const unsigned long DURATION = (ON_TIME+FADE_TIME) * 10;

public:
	FadeOut() : fadePWM(100) { fadePWM.quantum = 13; }

	virtual void init() {
		d = 0;
		val = 0;
		fadePWM.reset(100);
		startFade = start = millis();
	}

	virtual bool show() {
		nowMs = millis();

		return true;
	}

	virtual bool done() {
		return nowMs - start > DURATION;
	}

	virtual void display() {
		displayDigit();

		if (nowMs - startFade < ON_TIME) {
			return;
		}

		if (nowMs - startFade > ON_TIME + FADE_TIME) {
			startFade = nowMs;
			fadePWM.reset(100);
			val = (val + 1) % 10;
			d = val;
			return;
		}

		long dutyCycle = (long) 100L * (FADE_TIME + startFade + ON_TIME - nowMs) / FADE_TIME;
		dutyCycle = dutyCycle * dutyCycle / 100;
		fadePWM.onPercent = dutyCycle;

		if (fadePWM.off()) {
			d = 10;
		} else {
			d = val;
		}
	}
};

class FadeIn : public Display {
	SoftPWM fadePWM;
	int val;
	unsigned long startFade;
	unsigned long start;
	const unsigned long ON_TIME = 600;
	const unsigned long FADE_TIME = 400;
	const unsigned long INTERVAL = 5;
	const unsigned long DURATION = (ON_TIME+FADE_TIME) * 10;

public:
	FadeIn() : fadePWM(0) { fadePWM.quantum = 13; }

	virtual void init() {
		d = 9;
		val = 9;
		fadePWM.reset(0);
		startFade = start = millis();
	}

	virtual bool show() {
		nowMs = millis();

		return true;
	}

	virtual bool done() {
		return nowMs - start > DURATION;
	}

	virtual void display() {
		displayDigit();

		if (nowMs - startFade > ON_TIME + FADE_TIME) {
			startFade = nowMs;
			fadePWM.reset(0);
			val = (val + 9) % 10;
			d = val;
			return;
		}

		if (nowMs - startFade > FADE_TIME) {
			return;
		}

		long dutyCycle = (long) 100L * (nowMs - startFade) / FADE_TIME;
		dutyCycle = dutyCycle * dutyCycle / 100;
		fadePWM.onPercent = dutyCycle;

		if (fadePWM.off()) {
			d = 10;
		} else {
			d = val;
		}
	}
};

class DisplayNumber: public Display {
public:
	virtual void init() {
		repCount = 0;
		interval = 0;
		maxReps = 0;
		d = 0;
	}

	virtual void display() {
		displayDigit();
	}

	void setDigit(int d) {
		this->d = d;
	}
};

Count count;
Riffle riffle;
SlowDown slowDown;
RandomDisplay randomDisplay;
FadeOut fadeOut;
FadeIn fadeIn;
DisplayNumber displayNumber;

Display *pDisplay[] = { &slowDown, &fadeOut, &riffle, &slowDown, &riffle, &fadeIn, &riffle, &randomDisplay, &riffle  };
#define LOOP_START 1
#define NUM_DISPLAYS 9

byte mode = 0;

void setMode(byte newMode) {
	pDisplay[newMode]->init();
	mode = newMode;
}

void changeMode() {
	do {
		mode = random(NUM_DISPLAYS);
	} while  (mode < LOOP_START);

	setMode(mode);
}

unsigned long onTimer = 0;
const unsigned long onDuration = 5000;
byte onCount = 0;

void setup() {
	// Change to 4 MHz by changing clock prescaler to 2
	cli();
	// Disable interrupts
	CLKPR = (1 << CLKPCE); // Prescaler enable
	CLKPR = (1 << CLKPS0); // Clock division factor 2 (0001)
	// Enable interrupts
	sei();

	pinMode(PIN_A, OUTPUT);
	pinMode(PIN_B, OUTPUT);
	pinMode(PIN_C, OUTPUT);
	pinMode(PIN_D, OUTPUT);
	pinMode(PIN_EN, OUTPUT);
	pinMode(PIN_SWITCH, INPUT_PULLUP);

	onTimer = millis();
	digitalWrite(PIN_EN, LOW);
	Display::displayOn = true;
	onCount = 1;

	setMode(0);
	pDisplay[mode]->display();
}

unsigned long displayPaused = 0;
const unsigned long pauseDelay = 500;
bool alwaysOn = false;

// The loop function is called in an endless loop
void loop() {
	unsigned long nowMs = millis();

	if (button.doubleClicked()) {
		alwaysOn = !alwaysOn;
		Display::displayOn = alwaysOn;
		onCount = 0;
		displayNumber.setDigit(onCount);
		displayNumber.display();
		displayPaused = nowMs;
	}

	if (!alwaysOn && button.clicked()) {
		if (!(nowMs - onTimer > onDuration)) {
			// Timer still running, add to onCount
			onCount = min(9, onCount + 1);
		} else {
			onCount = 1;
			onTimer = nowMs;
		}
		digitalWrite(PIN_EN, LOW);	// Turn on HV
		if (Display::displayOn == false) {
			setMode(0);
		}
		Display::displayOn = true;
		displayNumber.setDigit(onCount);
		displayNumber.display();
		displayPaused = nowMs;
	}

	if (!alwaysOn && (nowMs - onTimer > onDuration)) {
		// On timer has expired
		if (onCount != 0 && --onCount > 0) {
			// Go for another cycle
			onTimer = nowMs;
		} else {
			// Turn off display
			digitalWrite(PIN_EN, HIGH);	// Turn off HV
			Display::displayOn = false;
		}
	}

	if (nowMs - displayPaused > pauseDelay) {
		if (pDisplay[mode]->show()) {
			if (pDisplay[mode]->done()) {
				changeMode();
			}
			pDisplay[mode]->display();
		}
	}
}
