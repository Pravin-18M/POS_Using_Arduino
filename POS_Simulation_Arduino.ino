#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc(SS_PIN, RST_PIN);

const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const int LED_PIN = A0;
LiquidCrystal_I2C *lcd = nullptr;

enum State { ENTER_AMOUNT, WAIT_CARD, SHOW_RESULT };
State state = ENTER_AMOUNT;

String amountStr = "";
unsigned long stateTimer = 0;

const String CARD_UID = "33:87:AC:A2";
long balance = 25000;

uint8_t scanI2CForDevice() {
  Wire.begin();
  for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) return addr;
  }
  return 0;
}

void createInitLCD(uint8_t addr) {
  if (lcd) { delete lcd; lcd = nullptr; }
  if (addr == 0) addr = 0x27;
  lcd = new LiquidCrystal_I2C(addr, 16, 2);
  lcd->init();
  lcd->backlight();
  delay(30);
}

String uidToString(MFRC522::Uid uid){
  String s = "";
  for (byte i = 0; i < uid.size; i++){
    if(uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
    if(i + 1 < uid.size) s += ":";
  }
  s.toUpperCase();
  return s;
}

void showEnterAmountScreen(){
  state = ENTER_AMOUNT;
  amountStr = "";
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Enter Sale");
  lcd->setCursor(0,1);

}

void updateAmountLine(){
  lcd->setCursor(0,1);
  if(amountStr.length() == 0) {
    lcd->print("0               ");
  } else {
    lcd->print(amountStr);
    for(int i = 0; i < 16 - (int)amountStr.length(); ++i) lcd->print(' ');
  }
}

void showPleaseTap(){
  state = WAIT_CARD;
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Please tap card");
  lcd->setCursor(0,1);
  if(amountStr.length()) lcd->print(amountStr); else lcd->print("0");
}

void showResultAndNewBalance(long newBal){
  state = SHOW_RESULT;
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Txn Successful");
  lcd->setCursor(0,1);
  lcd->print("New Bal:");
  lcd->print(newBal);
  stateTimer = millis() + 2000;
}

void showInsufficient(){
  state = SHOW_RESULT;
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Insufficient");
  lcd->setCursor(0,1);
  lcd->print("Funds");
  stateTimer = millis() + 2000;
}

void showUnknownCard(){
  state = SHOW_RESULT;
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Card not");
  lcd->setCursor(0,1);
  lcd->print("recognized");
  stateTimer = millis() + 2000;
}

void blinkLedSuccess(){
  for(int i=0;i<3;i++){
    digitalWrite(LED_PIN, HIGH);
    delay(170);
    digitalWrite(LED_PIN, LOW);
    delay(120);
  }
}

void setup(){
  Serial.begin(115200);
  while(!Serial) { }
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin();
  uint8_t addr = scanI2CForDevice();
  if(addr == 0){
    createInitLCD(0x27);
  } else {
    createInitLCD(addr);
  }

  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print("Initializing...");
  delay(150);
  SPI.begin();
  mfrc.PCD_Init();
  delay(100);
  showEnterAmountScreen();
  Serial.println("Mini POS ready");
  Serial.print("Authorized UID: ");
  Serial.println(CARD_UID);
  Serial.print("Starting balance: ");
  Serial.println(balance);
}

void loop(){
  if(state == ENTER_AMOUNT){
    char k = keypad.getKey();
    if(k){
      Serial.print("Key: ");
      Serial.println(k);
      if(k >= '0' && k <= '9'){
        if(amountStr.length() < 8){
          if(amountStr == "0") amountStr = String(k);
          else amountStr += k;
        }
      } else if(k == '*'){
        amountStr = "";
        lcd->clear();
        lcd->setCursor(0,0);
        lcd->print("Please enter");
        lcd->setCursor(0,1);
        lcd->print("0");
      } else if(k == '#'){
        if(amountStr.length() == 0) amountStr = "0";
        showPleaseTap();
        Serial.print("Amount confirmed: ");
        Serial.println(amountStr);
      }
      updateAmountLine();
    }
  }
  else if(state == WAIT_CARD){
    if (mfrc.PICC_IsNewCardPresent() && mfrc.PICC_ReadCardSerial()){
      String id = uidToString(mfrc.uid);
      Serial.print("Card tapped UID: ");
      Serial.println(id);
      if(id == CARD_UID){
        long amt = amountStr.length()? amountStr.toInt() : 0;
        if(amt <= balance){
          balance -= amt;
          Serial.print("TXN SUCCESS Amount=");
          Serial.print(amt);
          Serial.print(" NewBal=");
          Serial.println(balance);
          blinkLedSuccess();
          showResultAndNewBalance(balance);
        } else {
          Serial.println("INSUFFICIENT FUND");
          showInsufficient();
        }
      } else {
        Serial.println("UNKNOWN CARD");
        showUnknownCard();
      }
      mfrc.PICC_HaltA();
      mfrc.PCD_StopCrypto1();
    }
  }
  else if(state == SHOW_RESULT){
    if(millis() >= stateTimer){
      showEnterAmountScreen();
    }
  }
}
