#include <ArduinoSTL.h>  //necesar pentru deque
#include "LedControl.h" //  need the library
#include <deque>  //double - ended - queue este dpdv practic un vector care are si pop_front()
#include "LiquidCrystal.h"
using namespace std;  //ca sa nu mai scriu std::

LedControl lc = LedControl(12, 11, 10, 1);  // 

const int rs = 8, en = 9, d4 = 5, d5 = 4, d6 = 3, d7 = 2; 
LiquidCrystal lcd(rs, en, d4, d5, d6, d7); 

 
// pin 12 is connected to the MAx7219 pin 1
// pin 11 is connected to the CLK pin 13
// pin 10 is connected to LOAD pin 12
// 1 as we are only using 1 MAx7219

//Joystick
const int xPin = 0;  // analog pin connected to x output

const int buttonPin = 7;      // the number of the pushbutton pin
bool buttonState;  //vom citi starea butonul pentru a stii daca tragem sau nu

unsigned long currentTime = 0;  //timpul curent
unsigned long menuTime = 0;  //timpul petrecut in meniu, pana apasam butonul de start

unsigned long reload = 0;  //trebuie sa asteptam pentru a trage un foc

unsigned long timeCreateAst;  //ultimata data cand s - a creat un asteroid

unsigned long prevMoveTime = 0;  //ultima data cand s - a miscat nava

deque<unsigned long> prevProjTime;  //tinem minte ultima data cand s - a miscat fiecare proiectil

deque<unsigned long> prevAstTime;  //tinem minte ultima data cand s - a miscat fiecare asteroid
bool inUse[8];  //cand se creaya un asteroid nou mai intai verific daca exista unul acolo deja

int xAxis;  //in variabila asta citim axa x de pe joystick
int level = 1; 

int highScore = 0; 
int score = 0; 

struct spaceship {
    int hp; 
    int lives; 
    int x, y; ///col, row
    int dmg; ///damage
    int wep; ///ce arma detine, cu cat nr e mai mare cu atat e mai buna/diferita
    bool hit; 
    unsigned long prevFlashTime; 
    int flashIterations; 
     void setLEDsOff() {
      lc.setLed(0, x, y, false); 
      lc.setLed(0, x+1, y, false); 
      lc.setLed(0, x, y+1, false); 
      lc.setLed(0, x - 1, y, false); 
    }
     void setLEDsOn() {
      lc.setLed(0, x, y, true); 
      lc.setLed(0, x+1, y, true); 
      lc.setLed(0, x, y+1, true); 
      lc.setLed(0, x - 1, y, true); 
    }
    spaceship() {
       score = 0; 
       hp = 500+75*level; 
       lives = 3; 
       x = 3; 
       y = 0; 
       wep = 1; 
       dmg = 100; 
       prevFlashTime = 0; 
       flashIterations = 2; 
       hit = 0; 
    }
    ~spaceship() {}
}; 

spaceship ship, initialShip;  //creem player - ul si o copie initiala, de fiecare data cand reincepem jocul facem ship = initialShip

struct asteroid {
  int x, y; 
  int hp; 
  int dmg; 
  int size; 
  void setLEDsOff() {
    for (int i = x; i < x+size; i++)
      for (int j = y; j < y+size; j++)
        lc.setLed(0, i, j, false); 
  }
  void setLEDsOn() {
    for (int i = x; i < x+size; i++)
      for (int j = y; j < y+size; j++)
        lc.setLed(0, i, j, true); 
  }
  asteroid() {
    size = random(1, level/2+1); 
    hp = sqrt(level)*size*50; 
    dmg = level*size*50; 
  }
}; 

deque<asteroid> ast;

struct projectile {
  int x, y; 
  bool done; //daca a lovit un meteorit
  void setLEDsOn() { //in functie de ce arma detine player - ul proiectilul are for me diferite
    for (int i = y; i < y + ship.wep; i++)
      lc.setLed(0, x, i, true);
  }

  void setLEDsOff() {
    for (int i = y; i < y + ship.wep; i++)
      lc.setLed(0, x, i, false);
  }

  projectile() {
    done = false; 
    x = ship.x; 
    y = ship.y + 2; 
    setLEDsOn(); 
  }
}; 

deque<projectile> proj;  //double - ended - queue pentru proiectile, e folosit pentru a gestiona miscarea lor si disparitia lor din mapa (cand dispar de pe mapa datele vor si sterse) 

void refreshMatrix() { //functie pentru a actualiza starea si afisajul matricii
  for (int i = 0; i < proj.size(); i++) //pentru fiecare proiectil din proj
    if (currentTime - prevProjTime[i] >= 100 - (ship.wep - 1) * 20) { // miscam proiectilul din 100 in 100 de ms
      if (proj[i].done == false)
        proj[i].setLEDsOff();  //inchidem led - urile responsabile pentru pozitia anterioara
      proj[i].y = proj[i].y + 1;  //actualizam pozitia
      if (proj[i].done == false)
        proj[i].setLEDsOn();  //deschidem led - urile pentru pozitia noua
      prevProjTime[i] = currentTime;  //actualizam ultima data cand s - a miscat proiectilul
      if (proj[i].y > 7) { //daca a iesit din mapa
        proj.pop_front();  //il stergem
        i--; 
        prevProjTime.pop_front();  //si de asemenea si timpul lui specific va fi sters
      }
    }
  //idem pentru asteroizi
  for (int j = 0; j < ast.size(); j++) {
    //actualizarea pozitiei asteroizilor
    if (currentTime - prevAstTime[j] >= 750 / sqrt(sqrt(level))) {
      if (ast[j].hp > 0)
        ast[j].setLEDsOff(); 
      ast[j].y = ast[j].y - 1; 
      if (ast[j].hp > 0)
        ast[j].setLEDsOn(); 
      prevAstTime[j] = currentTime; 
      if (ast[j].y+ast[j].size - 1 < 0) { //cand marginea de sus a asteroidului a iesit din mapa, este eliminat din joc
        for (int i = ast[j].x; i < ast[j].x + ast[j].size; i++)
          inUse[i] = false; 
        ast.pop_front(); 
        j--; 
        prevAstTime.pop_front(); 
      }
    }
  }
  //nava palpaie cand e lovita si va palpai si mai mult cand pierde o viata
  if (ship.hit == true) {
    if (ship.flashIterations && currentTime - ship.prevFlashTime >= 50) {
      if (ship.flashIterations%2 == 0)
        ship.setLEDsOff(); 
      else ship.setLEDsOn(); 
      ship.flashIterations -= 1; 
      ship.prevFlashTime = currentTime; 
    }
    else if (ship.flashIterations == 0) { 
      ship.hit = false; 
      ship.flashIterations = 2; 
    }
  }
}

void showLossScreen(); //am declarat-o inaintea functiei urmatoare pentru ca e folosita in ea

void detectCollision() {
  for (int j = 0; j < ast.size(); j++) {
    //coliziunea dintre asteroizi si proiectile
    for (int i = 0; i < proj.size(); i++) {
      if (proj[i].done == false && ast[j].hp > 0) {
        if (proj[i].x >= ast[j].x && proj[i].x < ast[j].x+ast[j].size && proj[i].y + ship.wep - 1 >= ast[j].y) {
          ast[j].hp -= ship.dmg * sqrt(ship.wep);  //scadem din viata asteroidului
          proj[i].done = true;  //proiectilul si - a indeplinit scopul
          proj[i].setLEDsOff();  //si dispare de pe mapa
          ast[j].setLEDsOn();  //inchiyand led - ul proiectilului la coliziune se v - a inchide si led - ul asteroidului asa ca il aprindem la loc
          if (ast[j].hp <= 0) {
            score += sqrt(level)*ast[j].size*50; 
            ast[j].setLEDsOff(); 
            for (int k = ast[j].x; k < ast[j].x + ast[j].size; k++)
              inUse[k] = false; 
          }
        }
      }
    }
    //coliziunea dintre asteroizi si nava
    if (ast[j].hp <= 0) //daca asteroidul e deja distrus nu mai are ce pagube sa produce si continuam verificarea coliyiunilor
      continue; 
    if (ast[j].x <= ship.x && ast[j].x+ast[j].size - 1 >= ship.x) { //coliziune pe botul navei
      if (ast[j].y == ship.y+1) {
        ship.hit = true; 
        ast[j].hp = 0;  //asteroidul e distrus
        for (int k = ast[j].x; k < ast[j].x+ast[j].size; k++)
          inUse[k] = false; 
        ast[j].setLEDsOff();  //stingem led - urile asteroidului
        ship.setLEDsOn();  //pentru ca se suprapun, vor fi stinse si led - urile navei asa ca le aprindem la loc
        ship.hp = ship.hp - ast[j].dmg;  //scadem din viata navei
        if (ship.hp <= 0) { //daca hp - ul e sub sau egal cu 0
          ship.hit = true; 
          ship.flashIterations = 8; 
          ship.lives = ship.lives - 1;  //scadem o viata
          if (ship.lives) //daca inca mai avem vieti
            ship.hp = 500+75*level;  //redam hp - ul navei
          else
            showLossScreen();  //altfel am pierdut
        }
      }
    }
    else if (ast[j].x == ship.x+1 || ast[j].x+ast[j].size - 1 == ship.x - 1) { //daca marginea din stanga a asteroidului se loveste de marginea din dreapta a navei si vice versa
      if (ast[j].y == ship.y) {
        ship.hit = true; 
        ast[j].hp = 0;  //asteroidul e distrus
        for (int k = ast[j].x; k < ast[j].x+ast[j].size; k++)
          inUse[k] = false; 
        ast[j].setLEDsOff();  //stingem led - urile asteroidului
        ship.setLEDsOn();  //pentru ca se suprapun, vor fi stinse si led - urile navei asa ca le aprindem la loc
        ship.hp = ship.hp - ast[j].dmg;  //scadem din viata navei
        if (ship.hp <= 0) { //daca hp - ul e sub 0
          ship.hit = true; 
          ship.flashIterations = 8; 
          ship.lives = ship.lives - 1;  //scadem o viata
          if (ship.lives) //daca inca mai avem vieti
            ship.hp = 500+75*level;  //redam hp - ul navei
          else
            showLossScreen();  //altfel am pierdut
        }
      }
    }
  }
}

void detectFire() { //functie de detectare a tragerii unui foc
  if (score >= 2000)
    ship.wep = 2;
  
  if (score >= 3500)
    ship.wep = 3;
  
  buttonState = digitalRead(buttonPin);  //citim butonul sa vedem daca e sau nu apasat (1/0)
  if (buttonState && currentTime - reload >= 400){ //daca e apasat si au trecut mai mult de 400ms de la ultimul foc
    projectile p;  //creem un nou proiectil
    proj.push_back(p);  //il adaugam in deque - ul de proiectile
    prevProjTime.push_back(currentTime);  //adaugam si timpul lui specific de actualiyare
    reload = currentTime;  //actualizam ultima data cand a fost tras un foc
  }
}

void detectMove() { //functie de detectare a miscarii navei
  xAxis = analogRead(xPin);  //citim analog starea joystickului pe axa x
  if ((xAxis < 500 || xAxis > 540) && currentTime - prevMoveTime >= 200) { //daca joystick - ul s - a miscat si au trecut cel putin 200ms de la ultima actualiyare a navei
    if (xAxis > 540 && ship.x < 6) { //in functie de valoarea din xAxis miscam nava in dreapta, daca se poate
      ship.setLEDsOff(); 
      ship.x = ship.x+1; 
      ship.setLEDsOn(); 
    }
    if (xAxis < 500 && ship.x > 1) { //respectiv in stanga, daca se poate
      ship.setLEDsOff(); 
      ship.x = ship.x - 1; 
      ship.setLEDsOn(); 
    }
    prevMoveTime = currentTime;  //actualizam ultima data cand s - a miscat nava
  }
}

void createAsteroids() {
  if (currentTime - timeCreateAst >= random(2000, 3501) / sqrt(level)) {
    asteroid a; 
    a.x = random(1, 7); 
    for (int i = a.x; i < a.x + a.size; i++)
      if (inUse[i])
        return; 
    for (int i = a.x; i < a.x + a.size; i++)
      inUse[i] = true; 
    a.y = 7; 
    a.setLEDsOn(); 
    ast.push_back(a); 
    prevAstTime.push_back(currentTime); 
    timeCreateAst = currentTime; 
  }
}


void clearForNextLvl() {
  lc.clearDisplay(0); 
  for (int i = 0; i < 8; i++)
    inUse[i] = false; 
  while (ast.size())
    ast.pop_back(); 
  while (proj.size())
    proj.pop_back(); 
  while (prevAstTime.size())
    prevAstTime.pop_back(); 
  while (prevProjTime.size())
    prevProjTime.pop_back(); 
}

int delayTime=500;

void showLevelScreen() {
  lc.clearDisplay(0); 
  byte L[8] = {B00000000, B00111100, B00100000, B00100000, B00100000, B00100000, B00100000, B00000000}; 
  byte E[8] = {B00000000, B00111100, B00100000, B00111100, B00100000, B00111100, B00000000, B00000000}; 
  byte V[8] = {B00000000, B00000000, B00010000, B00101000, B01000100, B10000010, B00000000, B00000000}; 
  byte LL[5][8] = {{B00000000, B00001000, B00011000, B00101000, B00001000, B00001000, B00001000, B00000000}, 
  {B00000000, B00011000, B00100100, B00000100, B00001000, B00010000, B00111110, B00000000}, 
  {B00000000, B00011100, B00100010, B00000100, B00100010, B00011100, B00000000, B00000000}, 
  {B00000000, B00001000, B00011000, B00101000, B01111100, B00001000, B00001000, B00000000}, 
  {B00000000, B00111100, B00100000, B00111000, B00000100, B00001000, B00110000, B00000000}}; 

  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, L[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, E[i]); 
  delay(delayTime);  
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, V[i]); 
  delay(delayTime);  
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, E[i]); 
  delay(delayTime);  
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, L[i]); 
  delay(delayTime);  
  for (int i = 7; i >= 0; i-- )
    lc.setColumn(0, 7 - i, LL[level - 1][i]); 
  delay(delayTime); 
  clearForNextLvl(); 
  ship.setLEDsOn(); 
}

void clearInfo() {
  //se curata datele folosite de jocul anterior
  level = 1; 
  ship = initialShip; 
  score=0;
  lc.clearDisplay(0); 
  for (int i = 0; i < 8; i++)
    inUse[i] = false; 
  while (ast.size())
    ast.pop_back(); 
  while (proj.size())
    proj.pop_back(); 
  while (prevAstTime.size())
    prevAstTime.pop_back(); 
  while (prevProjTime.size())
    prevProjTime.pop_back(); 
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Highscore  Score");
  lcd.setCursor(0,1);
  lcd.print(highScore);
  lcd.setCursor(9,1);
  lcd.print(score);
  clearInfo(); 
  byte P[8] = {B00000000, B00100000, B00100000, B00100000, B00111100, B00100100, B00111100, B00000000}; 
  byte R[8] = {B00000000, B00100100, B00101000, B00110000, B00111100, B00100100, B00111100, B00000000}; 
  byte E[8] = {B00000000, B00111100, B00100000, B00111100, B00100000, B00111100, B00000000, B00000000}; 
  byte S[8] = {B00000000, B00000000, B00111100, B00000100, B00111100, B00100000, B00111100, B00000000}; 

  byte B[8] = {B00000000, B00111100, B00100100, B00111000, B00100100, B00111100, B00000000, B00000000}; 
  byte U[8] = {B00000000, B00111100, B00100100, B00100100, B00100100, B00100100, B00000000, B00000000}; 
  byte T[8] = {B00000000, B00001000, B00001000, B00001000, B00001000, B00001000, B00111110, B00000000}; 
  byte O[8] = {B00000000, B00111100, B00100100, B00100100, B00100100, B00100100, B00111100, B00000000}; 
  byte N[8] = {B00000000, B00100010, B00100110, B00101010, B00110010, B00110010, B00100010, B00000000}; 

  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, P[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, R[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, E[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, S[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, S[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, B[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, U[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, T[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, T[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, O[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  delay(delayTime/10); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, N[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  while (!digitalRead(buttonPin)) {
    digitalRead(buttonPin); 
  }
  menuTime = millis();
  lcd.clear();
}

void showLossScreen() {
  lc.clearDisplay(0); 
  byte L[8] = {B00000000, B00111100, B00100000, B00100000, B00100000, B00100000, B00100000, B00000000}; 
  byte O[8] = {B00000000, B00111100, B00100100, B00100100, B00100100, B00100100, B00111100, B00000000}; 
  byte S[8] = {B00000000, B00000000, B00111100, B00000100, B00111100, B00100000, B00111100, B00000000}; 
  byte T[8] = {B00000000, B00001000, B00001000, B00001000, B00001000, B00001000, B00111110, B00000000}; 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, L[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, O[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, S[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, T[i]); 
  delay(delayTime); 
  lc.clearDisplay(0); 
  showMenu(); 
}

void showWinScreen() {
  lc.clearDisplay(0); 
  byte W[8] = {B00000000, B00100010, B00110110, B01101011, B01101011, B01000001, B01000001, B00000000}; 
  byte O[8] = {B00000000, B00111100, B00100100, B00100100, B00100100, B00100100, B00111100, B00000000}; 
  byte N[8] = {B00000000, B00100010, B00100110, B00101010, B00110010, B00110010, B00100010, B00000000}; 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, W[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, O[i]); 
  delay(delayTime); 
  for (int i = 0; i < 8; i++)
    lc.setColumn(0, i, N[i]); 
  delay(delayTime); 
  lc.clearDisplay(0);
  showMenu();
}

int prevLevel = 0; 

void fetchLevel() {
  if (currentTime - menuTime <= 20000) { //nivelul 1 tine 20 secunde
    level = 1; 
    if (prevLevel != level)
      showLevelScreen(); 
    prevLevel = 1; 
  }
  else if (currentTime - menuTime <= 50000) { //nivelul 2 tine 30 secunde
         level = 2; 
         if (prevLevel != level)
           showLevelScreen(); 
         prevLevel = 2; 
       }
       else if (currentTime - menuTime <= 80000) { //nivelul 3 tine 30 secunde
               level = 3; 
               if (prevLevel != level)
                 showLevelScreen(); 
               prevLevel = 3;
            }
            else if (currentTime - menuTime <= 110000) { //nivelul 4 tine 30 secunde
                   level = 4; 
                   if (prevLevel != level)
                     showLevelScreen(); 
                   prevLevel = 4;
                 }
                 else if (currentTime - menuTime <= 140000) { //nivelul 5 tine 30 secunde
                        level = 5; 
                        if (prevLevel != level)
                          showLevelScreen(); 
                        prevLevel = 5;
                        
                      }
                      else if (currentTime - menuTime <= 170000) {
                        showWinScreen();
                      }
                     
}

void setup() {
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2); 
  
  // the zero refers to the MAx7219 number, it is zero for  1 chip
  lc.shutdown(0, false); // turn off power saving, enables display
  lc.setIntensity(0, 8); // sets brightness (0~15 possible values)
  lc.clearDisplay(0); // clear screen
  showMenu(); 
  ship.setLEDsOn(); 
  randomSeed(analogRead(5)); // pt ca nu e nimic conectat pe pinul A5 randomSeed() va genera un seed diferit de fiecare data cand rulam programul
  Serial.begin(9600);       // open the serial port at 9600 bps:
}

void loop() {
  // put your main code here, to run repeatedly:
  //Actualizarea poyitiei navei spatiale
  currentTime = millis();  //citim timpul curent
  
  fetchLevel();
  detectMove();  //detectam miscare
  createAsteroids();
  detectFire();  //detectam focuri
  detectCollision();  //detectam coliziune intre nava, asteroizi si proiectile
  refreshMatrix();  //actualizam matricea
  
  lcd.setCursor(0, 0); 
  lcd.print("Score  HP  Lives"); 

  lcd.setCursor(0, 1); 
  lcd.print(score); 

  lcd.setCursor(6, 1); 
  lcd.print(ship.hp); 

  lcd.setCursor(12, 1); 
  lcd.print(ship.lives);   
  
  if (highScore < score)
    highScore = score; 
}
