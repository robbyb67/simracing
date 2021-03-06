/* 
    Race control box for iRacing motorsport simulation.
    You must select Keyboard + Joystick + Serial from the "Tools > USB Type" 
    menu.
    Copyright (C) 2019  Robert Bausdorf

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/
#include <Bounce.h>
#include <Encoder.h>
#include "config.h"

// Create Bounce objects for each button.  The Bounce object
// automatically deals with contact chatter or "bounce", and
// it makes detecting changes very simple.

Bounce btnBb1 = Bounce(PIN_BTN_BB1, BOUNCE_TIME);
Bounce btnBb2 = Bounce(PIN_BTN_BB2, BOUNCE_TIME);
Bounce btnRa1 = Bounce(PIN_BTN_RA1, BOUNCE_TIME);
Bounce btnRa2 = Bounce(PIN_BTN_RA2, BOUNCE_TIME);
Bounce btnFuel = Bounce(PIN_BTN_FUEL, BOUNCE_TIME);

Bounce btnBbCar = Bounce(PIN_BTN_BBCAR, BOUNCE_TIME);
Bounce btnBbPit = Bounce(PIN_BTN_BBPIT, BOUNCE_TIME);
Bounce btnBbFuel = Bounce(PIN_BTN_BBFUEL, BOUNCE_TIME);
Bounce btnBbTyre = Bounce(PIN_BTN_BBTYRE, BOUNCE_TIME);
Bounce btnBbPabs = Bounce(PIN_BTN_BBPABS, BOUNCE_TIME);
Bounce btnBbPrel = Bounce(PIN_BTN_BBPREL, BOUNCE_TIME);
Bounce btnBbLap = Bounce(PIN_BTN_BBLAP, BOUNCE_TIME);

Bounce btnExit = Bounce(PIN_BTN_EXIT, BOUNCE_TIME);
Bounce btnClearTy = Bounce(PIN_BTN_TY_CLEAR, BOUNCE_TIME);

Bounce swViewUp = Bounce(PIN_SW_VIEW_UP, BOUNCE_TIME);
Bounce swViewDn = Bounce(PIN_SW_VIEW_DN, BOUNCE_TIME);

Bounce swTyreUp = Bounce(PIN_SW_TY_FR, BOUNCE_TIME);
Bounce swTyreDn = Bounce(PIN_SW_TY_RL, BOUNCE_TIME);

Encoder bb1Enc = Encoder(PINS_ENC_BB1);
Encoder bb2Enc = Encoder(PINS_ENC_BB2);
Encoder ra1Enc = Encoder(PINS_ENC_RA1);
Encoder ra2Enc = Encoder(PINS_ENC_RA2);
Encoder fuelEnc = Encoder(PINS_ENC_FUEL);

struct Config {
  int encSteps;
  int ledBrightness;
  bool joystickMode;
}; 

struct PitSvFlags {
    byte lf_tire_change;
    byte rf_tire_change;
    byte lr_tire_change;
    byte rr_tire_change;
    byte fuel_fill;
    byte windshield_tearoff;
    byte fast_repair;
};

struct PitCommand {
    int clear;       // Clear all pit checkboxes
    int ws;          // Clean the winshield, using one tear off
    int fuel;        // Add fuel, optionally specify the amount to add in liters or pass '0' to use existing amount
    int lf;          // Change the left front tire, optionally specifying the pressure in KPa or pass '0' to use existing pressure
    int rf;          // right front
    int lr;          // left rear
    int rr;          // right rear
    int clear_tires; // Clear tire pit checkboxes
    int fr;          // Request a fast repair
    int clear_ws;    // Uncheck Clean the winshield checkbox
    int clear_fr;    // Uncheck request a fast repair
    int clear_fuel;  // Uncheck add fuel
};

Config config = { ENC_STEPS, LED_BRIGHTNESS, JOYSTICK_MODE};
PitSvFlags pitFlags = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40};
PitCommand pitCmd = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

int rActive = -1;
int amountFuel = 0;
int changeTyreR, changeTyreF, doRefill = 0;

void handleJoyButtonPress(int btn) {
  Joystick.button(btn, 1);
  delay(JOY_BTN_DURATION);
  Joystick.button(btn, 0);
}

void handleButtonJoy(Bounce* btn, int bntNum) {
  if(btn->fallingEdge()) {
    Joystick.button(bntNum, 1);
//    Keyboard.print(String(bntNum) + "-1");
  }
  if(btn->risingEdge()) {
    Joystick.button(bntNum, 0);
//    Keyboard.print(String(bntNum) + "-0");
  }
}

void handleButtonKey(Bounce* btn, int keyNum) {
  if(btn->fallingEdge()) {
    Keyboard.press(keyNum);
  }
  if(btn->risingEdge()) {
    Keyboard.release(keyNum);
  }
}

void handleBlackBoxBtn(Bounce*  btn, int btnNum, int relayPin) {
  handleButtonJoy(btn, btnNum);
  if( btn->fallingEdge() ) {
    if( rActive > 0 ) {
      digitalWrite(rActive, HIGH);
    }
    if( rActive != relayPin) {
      digitalWrite(relayPin, LOW);
      rActive = relayPin;
    } else {
      rActive = -1;
    }
  }
}

void handleEncoderJoy(Encoder* enc, int up, int down) {
  long encVal = enc->read();
  if(encVal < (-1 * config.encSteps)) {
    handleJoyButtonPress(down);
    enc->write(0);
  } else if( encVal > config.encSteps ) {
    handleJoyButtonPress(up);
    enc->write(0);
  }
}

void sendMacro(String macro) {
  Serial.print("#" + macro + "*\n");
}

void sendPitCmd(int cmd) {
  sendMacro(String(TXT_PCM_TELEGRAM) + String(cmd));
}

void handleButtonMacro(Bounce*  btn, String macro) {
  if(btn->risingEdge()) {
    sendMacro(macro);
  }
}

void handleTyreButtons() {
  if(btnClearTy.fallingEdge()) {
    Joystick.button(JOY_BTN_TY_CLEAR, 1);
  }
  if(btnClearTy.risingEdge()) {
    Joystick.button(JOY_BTN_TY_CLEAR, 0);
    sendPitCmd(pitCmd.clear_tires); 
    changeTyreF = 0;
    changeTyreR = 0;
  }

  if( digitalRead(PIN_SW_O_R) ) {
    if(swTyreUp.risingEdge()) {
      handleJoyButtonPress(JOY_BTN_TY_RIGHT);
      sendPitCmd(pitCmd.rf); 
      sendPitCmd(pitCmd.rr); 
      changeTyreF = 1;
    }
    if(swTyreDn.risingEdge()) {
      handleJoyButtonPress(JOY_BTN_TY_LEFT);
      sendPitCmd(pitCmd.lf); 
      sendPitCmd(pitCmd.lr); 
      changeTyreR = 1;
    }
  } else {
    if(swTyreUp.risingEdge()) {
      handleJoyButtonPress(JOY_BTN_TY_FRONT);
      sendPitCmd(pitCmd.rf); 
      sendPitCmd(pitCmd.lf); 
      changeTyreF = 1;
    }
    if(swTyreDn.risingEdge()) {
      handleJoyButtonPress(JOY_BTN_TY_REAR);
      sendPitCmd(pitCmd.rr); 
      sendPitCmd(pitCmd.lr); 
      changeTyreR = 1;
    }
  }
}

void handleEncoderFuel(Encoder* enc) {
  long encVal = enc->read();
  if(encVal < (-1 * config.encSteps)) {
    amountFuel --;
    doRefill = 1; 
    sendMacro(TXT_PFU_TELEGRAM + String(amountFuel));
    handleJoyButtonPress(JOY_BTN_FUEL_DEC);
    enc->write(0);
  } else if( encVal > config.encSteps ) {
    amountFuel ++;
    doRefill = 1; 
    sendMacro(TXT_PFU_TELEGRAM + String(amountFuel));
    handleJoyButtonPress(JOY_BTN_FUEL_INC);
    enc->write(0);
  }
}

void handleFuel() {
  if(btnFuel.risingEdge()) {
    doRefill = 0; 
    handleJoyButtonPress(JOY_BTN_FUEL_CLEAR);
    sendPitCmd(pitCmd.clear_fuel);
  }
  handleEncoderFuel(&fuelEnc);
  
}

// Update the button bounce objects
void updateButtons() {
  btnBb1.update();
  btnBb2.update();
  btnRa1.update();
  btnRa2.update();
  btnFuel.update();
  
  btnBbCar.update();
  btnBbPit.update();
  btnBbFuel.update();
  btnBbTyre.update();
  btnBbPabs.update();
  btnBbPrel.update();
  btnBbLap.update();

  btnExit.update();
  btnClearTy.update();
  swViewUp.update();
  swViewDn.update();
  swTyreUp.update();
  swTyreDn.update();
}

void setup() {
  Serial.begin(9600); // USB is always 12 Mbit/sec

  if( config.encSteps == 0 ) {
    config.encSteps = ENC_STEPS;
    config.ledBrightness = LED_BRIGHTNESS;
    config.joystickMode = JOYSTICK_MODE;
  }

  pinMode(PIN_BTN_BBCAR, INPUT_PULLUP);
  pinMode(PIN_BTN_BBPIT, INPUT_PULLUP);
  pinMode(PIN_BTN_BBFUEL, INPUT_PULLUP);
  pinMode(PIN_BTN_BBTYRE, INPUT_PULLUP);
  pinMode(PIN_BTN_BBPABS, INPUT_PULLUP);
  pinMode(PIN_BTN_BBPREL, INPUT_PULLUP);
  pinMode(PIN_BTN_BBLAP, INPUT_PULLUP);
  pinMode(PIN_BTN_BB1, INPUT_PULLUP);
  pinMode(PIN_BTN_BB2, INPUT_PULLUP);
  pinMode(PIN_BTN_RA1, INPUT_PULLUP);
  pinMode(PIN_BTN_RA2, INPUT_PULLUP);
  pinMode(PIN_BTN_FUEL, INPUT_PULLUP);
  pinMode(PIN_BTN_EXIT, INPUT_PULLUP);
  pinMode(PIN_BTN_TY_CLEAR, INPUT_PULLUP);
  pinMode(PIN_SW_TY_FR, INPUT_PULLUP);
  pinMode(PIN_SW_TY_RL, INPUT_PULLUP);
  pinMode(PIN_SW_VIEW_DN, INPUT_PULLUP);
  pinMode(PIN_SW_VIEW_UP, INPUT_PULLUP);
  pinMode(PIN_SW_O_R, INPUT_PULLUP);

  pinMode(PIN_LED_T_FR, OUTPUT);
  pinMode(PIN_LED_T_RL, OUTPUT);
  pinMode(PIN_LED_FUEL, OUTPUT);

  for( int i = 0; i < 8; i++ ) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);
  }

  bb1Enc.write(0);
  bb2Enc.write(0);
  ra1Enc.write(0);
  ra2Enc.write(0);
  fuelEnc.write(0);

  Joystick.useManualSend(false);
}

void normalOpJoystick() {
  // Update all the buttons.  There should not be any long
  // delays in loop(), so this runs repetitively at a rate
  // faster than the buttons could be pressed and released.
  updateButtons();

  handleEncoderJoy(&bb1Enc, JOY_BTN_BB1_INC, JOY_BTN_BB1_DEC);
  handleEncoderJoy(&bb2Enc, JOY_BTN_BB2_INC, JOY_BTN_BB2_DEC);
  handleEncoderJoy(&ra1Enc, JOY_BTN_RA1_INC, JOY_BTN_RA1_DEC);
  handleEncoderJoy(&ra2Enc, JOY_BTN_RA2_INC, JOY_BTN_RA2_DEC);
  handleButtonJoy(&btnBb1, JOY_BTN_BB1_TOGGLE);
  handleButtonJoy(&btnBb2, JOY_BTN_BB2_TOGGLE);
  handleButtonJoy(&btnRa1, JOY_BTN_RA1_TOGGLE);
  handleButtonJoy(&btnRa2, JOY_BTN_RA2_TOGGLE);
  handleButtonJoy(&swViewUp, JOY_BTN_VIEW_UP);
  handleButtonJoy(&swViewDn, JOY_BTN_VIEW_DN);
  
  handleBlackBoxBtn(&btnBbPit, JOY_BTN_BB_PIT, PIN_R6);
  handleBlackBoxBtn(&btnBbCar, JOY_BTN_BB_CAR, PIN_R7);
  handleBlackBoxBtn(&btnBbFuel, JOY_BTN_BB_FUEL, PIN_R5);
  handleBlackBoxBtn(&btnBbTyre, JOY_BTN_BB_TYRE, PIN_R4);
  handleBlackBoxBtn(&btnBbPabs, JOY_BTN_BB_PABS, PIN_R2);
  handleBlackBoxBtn(&btnBbPrel, JOY_BTN_BB_PREL, PIN_R3);
  handleBlackBoxBtn(&btnBbLap, JOY_BTN_BB_LAP, PIN_R1);

  handleTyreButtons();
  handleFuel();

  handleButtonKey(&btnExit, KEY_EXIT);
}

String readSerial() {
  char buff[10];
  int dataCount = 0;
  boolean startData = false;
  while(Serial.available()) {
    char c = Serial.read();
    if( c == '#' ) {
      startData = true;
    } else if( startData && dataCount < 10) {
      if( c != '*') {
        buff[dataCount++] = c;
      } else {  
        break;
      }
    } else if(dataCount >= 10) {
      return String();
    }
  }
  if( startData || dataCount > 0 ) {
    return String(buff);
  }
  return String();
}

void processFlags(int flags) {
  if( digitalRead(PIN_SW_O_R) ) {
    if(flags & pitFlags.rf_tire_change || flags & pitFlags.rr_tire_change) {
      changeTyreF = 1;
    } else {
      changeTyreF = 0;
    }
    if(flags & pitFlags.lf_tire_change || flags & pitFlags.lr_tire_change) {
      changeTyreR = 1;
    } else {
      changeTyreR = 0;
    }
  } else {
    if(flags & pitFlags.rf_tire_change || flags & pitFlags.lf_tire_change) {
      changeTyreF = 1;
    } else {
      changeTyreF = 0;
    }
    if(flags & pitFlags.lr_tire_change || flags & pitFlags.rr_tire_change) {
      changeTyreR = 1;
    } else {
      changeTyreR = 0;
    }
  }
  if( flags & pitFlags.fuel_fill ) {
    doRefill = 1;
  } else {
    doRefill = 0;
  }
}

void processFuel(float fuel) {
  amountFuel = int(fuel);
}

void processTelegram(String* telegram) {
  int idx = telegram->indexOf('=');
  if( idx > 0 ) {
    String key = telegram->substring(0, idx);
    String val = telegram->substring(idx+1);
    if( key.equals("PFL") ){
      processFlags(val.toInt());
    } else if( key.equals("PFU") ) {
      processFuel(val.toFloat());
    }
  }
}

void setLED() {
  if( changeTyreF ) {
    analogWrite(PIN_LED_T_FR, config.ledBrightness);
  } else {
    analogWrite(PIN_LED_T_FR, 0);
  }
  if( changeTyreR ) {
    digitalWrite(PIN_LED_T_RL, HIGH);
  } else {
    digitalWrite(PIN_LED_T_RL, LOW);
  }

  if( doRefill != 0 ) {
    analogWrite(PIN_LED_FUEL, config.ledBrightness);
  } else {
    analogWrite(PIN_LED_FUEL, 0);
  }
}

void loop() {
  normalOpJoystick();

  String telegram = readSerial();
  if(telegram.length() > 0) {
    processTelegram(&telegram);
  }

  setLED();
  
  delay(LOOP_DELAY);
}
