#include <GyverPortal.h>
#include <EEPROM.h>

struct LoginPass {
  char ssid[20];
  char pass[20];
  char poyas[3];
  char tokenBot[90];
  char chatIdP[160];
  char thingspeakAPIKey[20];
};

LoginPass lp;

void build() {
  GP.BUILD_BEGIN();
  GP.THEME(GP_DARK);
  GP.TITLE("Настройка АэроПоники","title1",GP_CYAN_B);
  GP.FORM_BEGIN("/login");
  GP.LABEL("Название WiFi:","lable1",GP_GREEN_B,0,true);
  GP.BREAK();
  GP.TEXT("lg", "WiFi логин", lp.ssid);
  GP.BREAK();
  GP.LABEL("Пароль WiFi:","lable2",GP_GREEN_B,0,true);
  GP.BREAK();
  GP.TEXT("ps", "WiFi пароль", lp.pass);
  GP.BREAK();
  GP.LABEL("Часовой пояс:","lable3",GP_GREEN_B,0,true);
  GP.BREAK();
  GP.TEXT("chp", "Часовой пояс", lp.poyas);
  GP.BREAK();
  GP.LABEL("Токен бота:","lable4",GP_GREEN_B,0,true);
  GP.BREAK();
  GP.TEXT("token", "Токен бота", lp.tokenBot);
  GP.BREAK();
  GP.LABEL("Chat_ID пользователей:","lable5",GP_GREEN_B,0,true);
  GP.BREAK();
  GP.TEXT("chap", "Chat ID пользователей", lp.chatIdP);
  GP.BREAK();
  GP.LABEL("ThingSpeak API:", "label6", GP_GREEN_B, 0, true);
  GP.BREAK();
  GP.TEXT("ts_key", "API Key", lp.thingspeakAPIKey);
  /*GP.BREAK();
  GP.CHECK("puk");
  GP.LABEL("GEWQ");
  GP.BREAK(); 
  GP.SWITCH("puk2");*/
  GP.SUBMIT("Сохранить");
  GP.FORM_END();

  GP.BUILD_END();
}

#include <WiFi.h>
#include "ThingSpeak.h"
#include <FastBot.h>
FastBot bot("xxx");

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
Adafruit_BME280 bme280;

#include <Adafruit_GFX.h>    
#include <Adafruit_ST7735.h> 
#include <SPI.h>
Adafruit_ST7735 tft = Adafruit_ST7735(5, 17, 16);

#include <NTPClient.h>
#include <WiFiUdp.h>
 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp3.stratum2.ru");
WiFiClient client;

#include <OneWire.h>

OneWire oneWire(4);

#define w_sensor 26
#define mosfet 27
#define fito_relay 25
#define term 4 
#define TDS 32
#define BBP 35

const int resistorValue = 10000;

bool flag_TG = false;
int index_TG = 0;
bool pumpEnabled = false;
bool autoPumpEnabled = true;
int pumpInterval = 5;      // Интервал между включениями в минутах
int pumpDuration = 15;      // Длительность работы в секундах
uint32_t pumpTimer = 0;
bool lastPumpState = false;

bool thingspeakEnabled;
unsigned long lastThingSpeakUpdate = 0;
const unsigned long thingSpeakInterval = 3600000;

bool lightEnabled = false;
bool autoLightEnabled = true;
int lightOnHour = 7;        // Час включения освещения
int lightOnMinute = 0;      // Минута включения освещения  
int lightOffHour = 23;      // Час выключения освещения
int lightOffMinute = 0;     // Минута выключения освещения

uint32_t timer_displey,timer_sensors;
uint32_t pumpStopTime;
uint32_t nextPumpTime;
bool water, bbp_state;
float mineral;
int t_water, t, h, p,Hour,Minute,Day,Year,Month;
String menu_start = "Доклад \t Архив \n Освещение \t Орошение";
String Chat_ID_DATA[15];

uint32_t water_alert_timer;
bool water_alert_sent = false;
bool last_water_state = true;
bool water_recovered_sent = false;

#define VREF 3.3    // Напряжение питания ESP32 (3.3V)

// Объявления функций
String utf8rus(String source);
void loginPortal();
void connectWiFi();
void newMsg(FB_msg& msg);
void display_dip();
void action(GyverPortal& p);
void checkBbpAlerts();
void sendBbpAlertToAll(String message);

int splitString(String input, char delimiter, String output[], int maxTokens) {
  int count = 0;
  int index = 0;

  while (input.length() > 0 && count < maxTokens) {
    index = input.indexOf(delimiter);
    if (index == -1) { // Если разделитель не найден
      output[count++] = input;
      break;
    }
    output[count++] = input.substring(0, index);
    input = input.substring(index + 1);
  }

  return count;
}



String utf8rus(String source)
{
  int i,k;
  String target;
  unsigned char n;
  char m[2] = { '0', '\0' };

  k = source.length(); i = 0;

  while (i < k) {
    n = source[i]; i++;

    if (n >= 0xC0) {
      switch (n) {
        case 0xD0: {
          n = source[i]; i++;
          if (n == 0x81) { n = 0xA8; break; }
          if (n >= 0x90 && n <= 0xBF) n = n + 0x30;
          break;
        }
        case 0xD1: {
          n = source[i]; i++;
          if (n == 0x91) { n = 0xB8; break; }
          if (n >= 0x80 && n <= 0x8F) n = n + 0x70;
          break;
        }
      }
    }
    m[0] = n; target = target + String(m);
  }
return target;
}

void setup() {
  Wire.begin();
  EEPROM.begin(450);
  EEPROM.get(0, lp);
  bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }
  tft.initR(INITR_BLACKTAB);     
  tft.cp437(true);
  
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK); 
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_CYAN);
  tft.println(utf8rus("   Загрузка"));
  tft.setTextSize(2); 
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(utf8rus("Подключение к"));
  tft.setCursor(0, 48);
  tft.println(utf8rus(lp.ssid));
  tft.setCursor(110, 75);
  tft.setTextColor(ST77XX_GREEN);
  tft.setTextSize(7);
  tft.write(0x96);

  pinMode(15, INPUT_PULLUP);
  pinMode(w_sensor, INPUT_PULLUP);
  pinMode(mosfet, OUTPUT);
  pinMode(fito_relay, OUTPUT);
  pinMode(term, OUTPUT);
  digitalWrite(mosfet, LOW);
   
  if (!digitalRead(15)){
  tft.fillScreen(ST77XX_BLACK);   
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(0, 0);
  tft.println(utf8rus("  Настройка"));
  tft.setTextSize(2); 
  tft.setCursor(0, 30);
  tft.setTextColor(ST77XX_GREEN);
  tft.println(utf8rus("Подключись к AeroGrow"));  
  loginPortal();
  } 
  
  connectWiFi();
  ThingSpeak.begin(client);
  
  tft.setCursor(110, 76);
  tft.write(0x99);
  bot.setPeriod(500);
  bot.setToken(lp.tokenBot);
  bot.attach(newMsg);
  splitString(String(lp.chatIdP), ',', Chat_ID_DATA, 15);
  Serial.println("Загружено Chat IDs:");
  for(int i = 0; i < 15; i++) {
  if(Chat_ID_DATA[i] != "") {
    Serial.println("ChatID[" + String(i) + "]: " + Chat_ID_DATA[i]);
  }
}
sensor_upd();
}

void loginPortal() {
  Serial.println("Portal start");

  // запускаем точку доступа
  WiFi.mode(WIFI_AP);
  WiFi.softAP("AeroGrow");

  // запускаем портал
  GyverPortal ui;
  ui.attachBuild(build);
  ui.start();
  ui.attach(action);

  // работа портала
  while (ui.tick());
}

void action(GyverPortal& p) {
  if (p.form("/login")) {      // кнопка нажата
    p.copyStr("lg", lp.ssid);  // копируем себе
    p.copyStr("ps", lp.pass);
    p.copyStr("chp", lp.poyas);
    p.copyStr("token", lp.tokenBot);
    p.copyStr("chap", lp.chatIdP);
    p.copyStr("ts_key", lp.thingspeakAPIKey);
    EEPROM.put(0, lp);              // сохраняем
    EEPROM.commit();                // записываем
    WiFi.softAPdisconnect();        // отключаем AP
    ESP.restart();
  }
}

void newMsg(FB_msg& msg) {
  String nn = "\n";
  String text = msg.text;
  String chat_id = msg.chatID;
  String user = msg.username;
  int i2 = 0;
  
  do{
    if (Chat_ID_DATA[i2] == chat_id) {
      if(text == "/start"){
        String welcome = "✅Добро пожаловать, " + user + nn;  
        welcome += "Вы авторизованы как пользователь системы AeroGrow.\nДоступные разделы меню:\n📊 Доклад – текущие показания датчиков\n💧 Орошение – управление поливом\n💡 Освещение – настройка света\n📁 Архив – данные в ThingSpeak\nСистема активна. Выберите действие ⤵️" + nn; 
        bot.showMenuText(welcome, menu_start, chat_id);    
      }

      if(text == "Архив"){
        bot.setTextMode(FB_MARKDOWN);
        bot.showMenuText("[Ссылка на Архив](https://thingspeak.mathworks.com/channels/3209391)", menu_start, chat_id); 
        bot.setTextMode(FB_TEXT);   
      }
      
      if(text == "Доклад"){
        String water_str;
        if(water == true){
          water_str = "полон";
        }else{
          water_str = "пуст";  
        }
        String dokl = "🗂Параметры🗂:" + nn; 
        dokl += "🌡Температура🌡: " + String(t) + "℃" + nn;
        dokl += "💧Влажность💧: " + String(h) + "%" + nn; 
        dokl += "🕰Давление🕰: " + String(p) + "мм.рт.ст" + nn;
        dokl += "💧Бак с раствором💧: " + String(water_str) + nn;
        dokl += "💧EC раствора💧: " + String(mineral) + "мСм/см" + nn;
        dokl += "🌡Температура корневой зоны🌡: " + String(t_water) + "℃" + nn;
        dokl += "⚡️Питание⚡️: " + String(bbp_state ? "Сеть" : "Аккумулятор") + nn;
        dokl += nn; 
        dokl += "💧Орошение💧:" + nn; 
        dokl += "🔸 Помпа: " + String(pumpEnabled ? "🟢ВКЛЮЧЕНА🟢" : "🔴ВЫКЛЮЧЕНА🔴") + nn;
        dokl += "🔸 Режим: " + String(autoPumpEnabled ? "АВТО" : "РУЧНОЙ") + nn;
        if(autoPumpEnabled) {
          dokl += "🔸 Интервал: " + String(pumpInterval) + " мин" + nn;
          dokl += "🔸 Длительность: " + String(pumpDuration) + " сек" + nn;
        }
        dokl += nn;
        dokl += "💡Освещение💡:" + nn;
        dokl += "🔸 Свет: " + String(lightEnabled ? "🟢ВКЛЮЧЕН🟢" : "🔴ВЫКЛЮЧЕН🔴") + nn;
        dokl += "🔸 Режим: " + String(autoLightEnabled ? "АВТО" : "РУЧНОЙ") + nn;
        if(autoLightEnabled) {
          dokl += "🔸 Включение: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute) + nn;
          dokl += "🔸 Выключение: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute) + nn;
        }
        bot.showMenuText(dokl, menu_start, chat_id);    
      }

      // МЕНЮ ОСВЕЩЕНИЯ
      if(text == "Освещение"){
        showLightMenu(chat_id);
      }
      
      // КОМАНДЫ ОСВЕЩЕНИЯ
      if(text == "Включить свет"){
        digitalWrite(fito_relay, HIGH);
        lightEnabled = true;
        bot.sendMessage("💡Свет включен💡", chat_id);
        showLightMenu(chat_id);
      }
      
      if(text == "Выключить свет"){
        digitalWrite(fito_relay, LOW);
        lightEnabled = false;
        bot.sendMessage("🔴Свет выключен🔴", chat_id);
        showLightMenu(chat_id);
      }
      
      if(text == "Сменить режим освещения"){
        autoLightEnabled = !autoLightEnabled;
        String modeMsg = autoLightEnabled ? "🟢Авторежим освещения включен🟢" : "🔴Ручной режим освещения🔴";
        bot.sendMessage(modeMsg, chat_id);
        showLightMenu(chat_id);
      }
      
      // НАСТРОЙКИ АВТОРЕЖИМА ОСВЕЩЕНИЯ
      if(text == "Настройки авторежима освещения"){
        showLightSettingsMenu(chat_id);
      }
      
      if(text == "Установить время включения"){
        bot.showMenuText("Текущее время включения: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute) + nn +
                        "Введите время в формате ЧЧ:ММ (например, 08:00):",
                        "Отмена",
                        chat_id);
        flag_TG = true;
        index_TG = 7;
      }
      
      if(text == "Установить время выключения"){
        bot.showMenuText("Текущее время выключения: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute) + nn +
                        "Введите время в формате ЧЧ:ММ (например, 20:00):",
                        "Отмена",
                        chat_id);
        flag_TG = true;
        index_TG = 8;
      }

      // МЕНЮ ОРОШЕНИЯ
      if(text == "Орошение"){
        showPumpMenu(chat_id);
      }
      
      // КОМАНДЫ ОРОШЕНИЯ
      if(text == "Включить помпу"){
        if (water) { // Проверяем наличие воды
          digitalWrite(mosfet, HIGH);
          pumpEnabled = true;
          bot.sendMessage("💧Помпа включена💧", chat_id);
          showPumpMenu(chat_id);
        } else {
          bot.sendMessage("❌ Невозможно включить помпу! Бак пуст! ❌", chat_id);
        }
      }
      
      if(text == "Выключить помпу"){
        digitalWrite(mosfet, LOW);
        pumpEnabled = false;
        bot.sendMessage("🔴Помпа выключена🔴", chat_id);
        showPumpMenu(chat_id);
      }
      
      if(text == "Сменить режим орошения"){
        autoPumpEnabled = !autoPumpEnabled;
        String modeMsg = autoPumpEnabled ? "🟢Авторежим орошения включен🟢" : "🔴Ручной режим орошения🔴";
        bot.sendMessage(modeMsg, chat_id);
        showPumpMenu(chat_id);
      }
      
      // НАСТРОЙКИ АВТОРЕЖИМА ОРОШЕНИЯ
      if(text == "Настройки авторежима орошения"){
        showPumpSettingsMenu(chat_id);
      }
      
      if(text == "Установить интервал"){
        bot.showMenuText("Текущий интервал: " + String(pumpInterval) + " мин" + nn +
                        "Введите новое значение интервала (1-120 минут):",
                        "Отмена",
                        chat_id);
        flag_TG = true;
        index_TG = 5;
      }
      
      if(text == "Установить длительность"){
        bot.showMenuText("Текущая длительность: " + String(pumpDuration) + " сек" + nn +
                        "Введите новое значение длительности (1-300 секунд):",
                        "Отмена",
                        chat_id);
        flag_TG = true;
        index_TG = 6;
      }

      
      // НАВИГАЦИЯ
      if(text == "Назад"){
        bot.showMenuText("Главное меню:", menu_start, chat_id);
        flag_TG = false;
      }
      
      if(text == "Отмена"){
        flag_TG = false;
        bot.sendMessage("❌Операция отменена❌", chat_id);
        bot.showMenuText("Главное меню:", menu_start, chat_id);
      }
      
      // ОБРАБОТКА ВВОДА ВРЕМЕНИ ДЛЯ ОСВЕЩЕНИЯ
      if(flag_TG && (index_TG == 7 || index_TG == 8) && text.indexOf(':') != -1){
        int colonIndex = text.indexOf(':');
        int hours = text.substring(0, colonIndex).toInt();
        int minutes = text.substring(colonIndex + 1).toInt();
        
        bool valid = true;
        String errorMsg = "";
        
        // Проверка допустимых значений
        if(hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
          valid = false;
          errorMsg = "❌ Неверное время! Введите время в формате ЧЧ:ММ (ЧЧ: 0-23, ММ: 0-59):";
        }
        
        if(!valid) {
          bot.showMenuText(errorMsg, "Отмена", chat_id);
          break;
        }
        
        // Установка значений времени
        if(index_TG == 7){
          lightOnHour = hours;
          lightOnMinute = minutes;
          bot.sendMessage("✅ Время включения освещения установлено: " + 
                         String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute), chat_id);
        }
        else if(index_TG == 8){
          lightOffHour = hours;
          lightOffMinute = minutes;
          bot.sendMessage("✅ Время выключения освещения установлено: " + 
                         String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute), chat_id);
        }
        
        flag_TG = false;
        showLightSettingsMenu(chat_id);
      }
      
      // ОБРАБОТКА ВВОДА ЧИСЕЛ ДЛЯ ОРОШЕНИЯ
      if(flag_TG && text.toInt() > 0 && (index_TG == 5 || index_TG == 6)){
        int value = text.toInt();
        bool valid = true;
        String errorMsg = "";
        
        // Проверка допустимых диапазонов для орошения
        if(index_TG == 5 && (value < 1 || value > 120)) {
          valid = false;
          errorMsg = "❌ Неверное значение! Введите число от 1 до 120 минут:";
        }
        else if(index_TG == 6 && (value < 1 || value > 300)) {
          valid = false;
          errorMsg = "❌ Неверное значение! Введите число от 1 до 300 секунд:";
        }
        
        if(!valid) {
          bot.showMenuText(errorMsg, "Отмена", chat_id);
          break;
        }
        
        // Установка значений для орошения
        if(index_TG == 5){
          pumpInterval = value;
          bot.sendMessage("✅ Интервал орошения установлен: " + String(pumpInterval) + " мин", chat_id);
        }
        else if(index_TG == 6){
          pumpDuration = value;
          bot.sendMessage("✅ Длительность орошения установлена: " + String(pumpDuration) + " сек", chat_id);
        }
        
        flag_TG = false;
        showPumpSettingsMenu(chat_id);
      }
      
      break;
    }
    if (i2==(sizeof(Chat_ID_DATA)/sizeof(String))&&text == "/start"||(sizeof(Chat_ID_DATA)/sizeof(String))==0&&text == "/start") {
      String spec_chatId = "Ваш ChatID: *" + chat_id + "*"; 
      bot.setTextMode(FB_MARKDOWN);
      String ww1 = "Вас приветствует бот Аэропоники, " + user + ".\n";
      ww1 += "🔐 Доступ к системе ограничен" + nn;
      ww1 += "Для подключения к системе введите CHAT ID в настройках устройства. \nПосле авторизации вы сможете:\n• Удалённо управлять установкой\n• Получать данные в реальном времени\n• Настраивать автоматические программы" + nn;
      ww1 += spec_chatId + nn;
      bot.sendMessage(ww1,chat_id);   
      bot.setTextMode(FB_TEXT);
      break;
    }
    ++i2; 
  } while (i2<=sizeof(Chat_ID_DATA)/sizeof(String)); 
}
void showPumpMenu(String chat_id) {
  String nn = "\n";
  String pumpMenu = "💧Орошение💧" + nn;
  pumpMenu += "Состояние помпы: " + String(pumpEnabled ? "🟢ВКЛЮЧЕНА🟢" : "🔴ВЫКЛЮЧЕНА🔴") + nn;
  pumpMenu += "Режим: " + String(autoPumpEnabled ? "АВТОМАТИЧЕСКИЙ" : "РУЧНОЙ") + nn;
  if(autoPumpEnabled && water) {
    if (pumpEnabled) {
      // Помпа работает - показываем оставшееся время работы
      unsigned long timeWorking = millis() - pumpTimer;
      unsigned long timeLeft = pumpDuration * 1000UL - timeWorking;
      if (timeLeft > pumpDuration * 1000UL) timeLeft = 0;
      
      int secondsLeft = timeLeft / 1000;
      pumpMenu += "Осталось работать: " + String(secondsLeft) + " сек" + nn;
    } else {
      // Помпа не работает - показываем время до следующего включения
      unsigned long timeUntilNext;
      if (nextPumpTime > millis()) {
        timeUntilNext = nextPumpTime - millis();
      } else {
        timeUntilNext = 0;
      }
      
      int minutesUntilNext = (timeUntilNext / 1000) / 60;
      int secondsUntilNext = (timeUntilNext / 1000) % 60;
      
      // Защита от переполнения
      if (minutesUntilNext > 999) minutesUntilNext = 0;
      if (secondsUntilNext > 59) secondsUntilNext = 0;
      
      pumpMenu += "Следующий запуск через: " + 
                 String(minutesUntilNext) + " мин " + 
                 String(secondsUntilNext) + " сек" + nn;
    }
    pumpMenu += "Интервал: " + String(pumpInterval) + " мин" + nn;
    pumpMenu += "Длительность: " + String(pumpDuration) + " сек" + nn;
  } else if (!water) {
    pumpMenu += "❌ Вода отсутствует! ❌" + nn;
  }
  
  String pumpOptions;
  if(autoPumpEnabled) {
    pumpOptions = "Сменить режим орошения \n Настройки авторежима орошения";
  } else {
    pumpOptions = "Сменить режим орошения \n Включить помпу \n Выключить помпу";
  }
  pumpOptions += " \n Назад";
  
  bot.showMenuText(pumpMenu, pumpOptions, chat_id);
}

// Функция для отображения настроек авторежима орошения
void showPumpSettingsMenu(String chat_id) {
  String nn = "\n";
  String pumpSettings = "💧Настройки авторежима орошения💧" + nn;
  pumpSettings += "Текущий интервал: " + String(pumpInterval) + " мин" + nn;
  pumpSettings += "Текущая длительность: " + String(pumpDuration) + " сек" + nn;
  pumpSettings += "Статус воды: " + String(water ? "🟢ЕСТЬ🟢" : "🔴НЕТ🔴") + nn;
  
  bot.showMenuText(pumpSettings,
                 "Установить интервал \n Установить длительность \n Назад",
                 chat_id);
}
void showLightMenu(String chat_id) {
  String nn = "\n";
  String lightMenu = "💡Освещение💡" + nn;
  lightMenu += "Состояние: " + String(lightEnabled ? "🟢ВКЛЮЧЕНО🟢" : "🔴ВЫКЛЮЧЕНО🔴") + nn;
  lightMenu += "Режим: " + String(autoLightEnabled ? "АВТОМАТИЧЕСКИЙ" : "РУЧНОЙ") + nn;
  if(autoLightEnabled) {
    lightMenu += "Включение: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute) + nn;
    lightMenu += "Выключение: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute) + nn;
    lightMenu += "Текущее время: " + String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute) + nn;
  }
  
  String lightOptions;
  if(autoLightEnabled) {
    lightOptions = "Сменить режим освещения \n Настройки авторежима освещения";
  } else {
    lightOptions = "Сменить режим освещения \n Включить свет \n Выключить свет";
  }
  lightOptions += " \n Назад";
  
  bot.showMenuText(lightMenu, lightOptions, chat_id);
}

// Функция для отображения настроек авторежима освещения
void showLightSettingsMenu(String chat_id) {
  String nn = "\n";
  String lightSettings = "💡Настройки авторежима освещения💡" + nn;
  lightSettings += "Включение: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute) + nn;
  lightSettings += "Выключение: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute) + nn;
  lightSettings += "Текущее время: " + String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute) + nn;
  
  bot.showMenuText(lightSettings,
                 "Установить время включения \n Установить время выключения \n Назад",
                 chat_id);
}

void loop() {
  bot.tick();

  if(millis()>=30000 && WiFi.status() != WL_CONNECTED){
    ESP.restart();  
  }
  
  static bool startMessagebot = true;
  if(startMessagebot){
    if (WiFi.status() == WL_CONNECTED)
  {
    for (int i = 0; i < sizeof(Chat_ID_DATA)/sizeof(String); i++){
    if(Chat_ID_DATA[i] != ""){
     bot.showMenuText("🟢АэроПоника СТАРТ!🟢", menu_start, Chat_ID_DATA[i]);  
    } 
  }
  }
  startMessagebot = false;
  }

  if(timer_sensors - millis() >= 5000){
  sensor_upd();
  timer_sensors = millis(); 
  }
  
  timeClient.update();
 time_t epochTime = timeClient.getEpochTime();
 
 Hour = timeClient.getHours();
 Serial.print("Hour: ");
 Serial.println(Hour); 
 
 Minute = timeClient.getMinutes();
 Serial.print("Minutes: ");
 Serial.println(Minute); 
 
 struct tm *ptm = gmtime ((time_t *)&epochTime); 
 
 Day = ptm->tm_mday;
 Serial.print("Month day: ");
 Serial.println(Day);
 
 Month = ptm->tm_mon+1;
 Serial.print("Month: ");
 Serial.println(Month);
 
 Year =  ptm->tm_year+1900;
 Serial.print("Year: ");
 Serial.println(Year);
 
  Serial.println("Air temperature = " + String(t) + " *C");
  Serial.println("Air humidity = " + String(h) + " %");
  Serial.println("Air pressure = " + String(p) + " мм.рт.ст");
  Serial.print("Water: ");
  Serial.println(water);
  Serial.print("Water_t: ");
  Serial.println(t_water);
  Serial.print("Mineral: ");
  Serial.println(mineral);
  Serial.print("BBP: ");
  Serial.println(bbp_state ? "220v" : "Acumulation");
  Serial.println(bbp_state);

   if (autoLightEnabled) {
      // Конвертируем всё в минуты от начала суток
      int currentTime = Hour * 60 + Minute;
      int startTime = lightOnHour * 60 + lightOnMinute;
      int endTime = lightOffHour * 60 + lightOffMinute;
      
      bool shouldLightBeOn = false;
      
      // ПРОВЕРКА: установлено ли время включения и выключения
      if (lightOnHour == lightOffHour && lightOnMinute == lightOffMinute) {
          // Если время включения и выключения одинаковое - освещение всегда выключено
          shouldLightBeOn = false;
      } 
      else if (startTime < endTime) {
          // НОРМАЛЬНЫЙ ИНТЕРВАЛ: включение раньше выключения в течение суток
          // Пример: 08:00 - 20:00
          shouldLightBeOn = (currentTime >= startTime && currentTime < endTime);
      } 
      else if (startTime > endTime) {
          // ИНТЕРВАЛ ЧЕРЕЗ ПОЛНОЧЬ: включение вечером, выключение утром
          // Пример: 20:00 - 08:00
          shouldLightBeOn = (currentTime >= startTime || currentTime < endTime);
      }
      // Если startTime == endTime - уже обработано в первом условии
      
      // Управляем реле только при изменении состояния
      if (shouldLightBeOn && !lightEnabled) {
          digitalWrite(fito_relay, HIGH);
          lightEnabled = true;
          Serial.println("Освещение включено (авто)");
          Serial.println("Текущее время: " + String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute));
          Serial.println("Включение: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute));
          Serial.println("Выключение: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute));
          
          // Отправка уведомлений всем пользователям
          for(int i = 0; i < 15; i++){
              if(Chat_ID_DATA[i] != ""){
                  bot.sendMessage("💡Освещение автоматически включено💡\nВремя: " + 
                                String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute), 
                                Chat_ID_DATA[i]);
              }
          }
      } 
      else if (!shouldLightBeOn && lightEnabled) {
          digitalWrite(fito_relay, LOW);
          lightEnabled = false;
          Serial.println("Освещение выключено (авто)");
          Serial.println("Текущее время: " + String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute));
          Serial.println("Включение: " + String(lightOnHour) + ":" + (lightOnMinute < 10 ? "0" : "") + String(lightOnMinute));
          Serial.println("Выключение: " + String(lightOffHour) + ":" + (lightOffMinute < 10 ? "0" : "") + String(lightOffMinute));
          
          // Отправка уведомлений всем пользователям
          for(int i = 0; i < 15; i++){
              if(Chat_ID_DATA[i] != ""){
                  bot.sendMessage("🔴Освещение автоматически выключено🔴\nВремя: " + 
                                String(Hour) + ":" + (Minute < 10 ? "0" : "") + String(Minute), 
                                Chat_ID_DATA[i]);
              }
          }
      }
  }
  //АВТОПОЛИВ
if (autoPumpEnabled) {
  if (!water) {
    // Если воды нет, выключаем помпу немедленно
    if (pumpEnabled) {
      digitalWrite(mosfet, LOW);
      pumpEnabled = false;
      Serial.println("Помпа выключена: вода отсутствует!");
    }
  } else {
    // Если вода есть
    // Если это первый запуск или помпа была выключена вручную
    if (nextPumpTime == 0 && !pumpEnabled) {
      nextPumpTime = millis() + pumpInterval * 60 * 1000UL;
    }
    
    // Проверяем время включения
    if (!pumpEnabled && millis() >= nextPumpTime) {
      // Включаем помпу
      digitalWrite(mosfet, HIGH);
      pumpEnabled = true;
      pumpTimer = millis();
      Serial.println("Помпа включена (авто)");
    }
    
    // Выключаем помпу после заданной длительности
    if (pumpEnabled && (millis() - pumpTimer >= pumpDuration * 1000UL)) {
      digitalWrite(mosfet, LOW);
      pumpEnabled = false;
      // Устанавливаем время следующего включения
      nextPumpTime = millis() + pumpInterval * 60 * 1000UL;
      Serial.println("Помпа выключена (авто)");
    }
  }
}
  
  
  if (!thingspeakEnabled || (millis() - lastThingSpeakUpdate >= thingSpeakInterval)) {
  sendToThingSpeak();
  lastThingSpeakUpdate = millis();
  }

  display_dip();
  checkWaterAlerts();
  checkBbpAlerts();
  checkECAlerts();
  checkTEMPAlerts();
}

void connectWiFi(){
  Serial.begin(115200);
  Serial.println();
  Serial.print("Подключение к WiFi: ");
  Serial.println(lp.ssid);
  
  WiFi.begin(lp.ssid, lp.pass);
  
  unsigned long wifiTimeout = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    // Таймаут подключения 15 секунд
    if (millis() - wifiTimeout > 15000) {
      Serial.println("\nТаймаут подключения WiFi!");
      ESP.restart();
    }
  }
  
  Serial.println("\nWiFi подключен!");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());

  // Инициализация времени с таймаутом
  timeClient.begin();
  timeClient.setTimeOffset(String(lp.poyas).toInt()*3600);
  
  // Ждем получения времени (но не блокируем надолго)
  unsigned long timeTimeout = millis();
  while(!timeClient.update() && millis() - timeTimeout < 5000) {
    delay(100);
  }
  
  if(timeClient.isTimeSet()) {
    Serial.println("Время синхронизировано");
  } else {
    Serial.println("Не удалось синхронизировать время");
  }
}

void sensor_upd(){
 if(isnan(bme280.readTemperature())){
     bool bme_status = bme280.begin();
  if (!bme_status) {
    Serial.println("Не найден по адресу 0х77, пробую другой...");
    bme_status = bme280.begin(0x76);
    if (!bme_status)
      Serial.println("Датчик не найден, проверьте соединение");
  }  
  }

  if(analogRead(BBP) < 1500){
  bbp_state = false;  
  }else if(analogRead(BBP) > 2000){
  bbp_state = true;  
  }
  
  t_water = readTemperature();
  mineral = getMineral();
 
  water = !digitalRead(w_sensor);
  t = bme280.readTemperature();
  h = bme280.readHumidity();
  p = (bme280.readPressure() / 100.0F) / 1.33; 
}

void display_dip(){
  String t_disp = "Темп:" + String(t); 
  String tw_disp = "Темп:" + String(t_water); 
  String h_disp = "Влаж:" + String(h) + "%"; 
  String p_disp = "Давл:" + String(p) + "mmHg";
  String ec_disp = "EC:" + String(mineral) + "мСм/см"; 
  String w_disp;
  if(water == true){
  w_disp = "Раствор есть";  
  }else{
  w_disp = "Раствора нет";   
  }
  static int sch;
  
  // Форматируем время с ведущим нулём для минут
  String minutes = (Minute < 10) ? "0" + String(Minute) : String(Minute);
  String houres = (Hour < 10) ? "0" + String(Hour) : String(Hour);
  String time2s = String(houres) + ":" + minutes;
  
  String dayStr = (Day < 10) ? "0" + String(Day) : String(Day);
  String monthStr = (Month < 10) ? "0" + String(Month) : String(Month);
  String time3s = dayStr + "." + monthStr + "." + String(Year);
  
  switch(sch){
    case 0:
      if(millis() - timer_displey >= 5000){
        tft.fillScreen(ST77XX_BLACK); 
        tft.setTextSize(2);
        tft.setCursor(4, 0);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(utf8rus("    Время  "));
        tft.setTextSize(3);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(36, 45);
        tft.println(time2s);
        tft.setTextSize(1);
        tft.setCursor(0, 17);
        tft.println("~~~~~~~~~~~~~~~~~~~~~~~~~~");
        tft.setTextSize(2);
        tft.setCursor(21, 90);
        tft.println(time3s);
        timer_displey = millis();
        sch = sch + 1; 
      }
      break;
    case 1:
      if(millis() - timer_displey >= 5000){
        tft.fillScreen(ST77XX_BLACK); 
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(utf8rus(" Микроклимат"));
        tft.setTextSize(2);
        tft.setCursor(0, 30);
        tft.setTextColor(ST77XX_GREEN);
        tft.print(utf8rus(t_disp));
        tft.write(0xB0);
        tft.print(utf8rus("C"));
        tft.setTextSize(2);
        tft.setCursor(0, 48);
        tft.println(utf8rus(h_disp));
        tft.setTextSize(2);
        tft.setCursor(0, 66);
        tft.println(utf8rus(p_disp));
        timer_displey = millis();
        sch = sch + 1; 
      }
      break;
      case 2:
      if(millis() - timer_displey >= 5000){
        tft.fillScreen(ST77XX_BLACK); 
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(utf8rus("   Раствор"));
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(0, 30);
        tft.println(utf8rus(w_disp));
        tft.setCursor(0, 48);
        tft.print(utf8rus(tw_disp));
        tft.write(0xB0);
        tft.print(utf8rus("C"));
        tft.setCursor(0, 66);
        tft.print(utf8rus(ec_disp));
        timer_displey = millis();
        sch = sch + 1;
      }
      break;
      case 3:
      if(millis() - timer_displey >= 5000){
        tft.fillScreen(ST77XX_BLACK); 
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(utf8rus("   Орошение"));
        
        // Основная информация
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(0, 30);
        tft.print(utf8rus("Режим:"));
        tft.println(autoPumpEnabled ? utf8rus("Авто") : utf8rus("Ручной"));
        
        // Следующий цикл
        tft.setCursor(0, 48);
        tft.println(utf8rus("След. цикл:"));
        
          if (autoPumpEnabled && water) {
          unsigned long timeUntilNext;
          
          if (pumpEnabled) {
            // Если помпа работает, показываем оставшееся время работы
            unsigned long timeWorking = millis() - pumpTimer;
            unsigned long timeLeft = pumpDuration * 1000UL - timeWorking;
            
            if (timeLeft > pumpDuration * 1000UL) {
              timeLeft = 0;  // Защита от переполнения
            }
            
            int minutesLeft = (timeLeft / 1000) / 60;
            int secondsLeft = (timeLeft / 1000) % 60;
            
            tft.setCursor(0, 66);
            tft.print(utf8rus("Работает: "));
            if (minutesLeft > 0) {
              tft.print(minutesLeft);
              tft.print(utf8rus("м "));
            }
            tft.print(secondsLeft);
            tft.println(utf8rus("с"));
            
          } else {
            // Если помпа не работает, показываем время до следующего включения
            if (nextPumpTime > millis()) {
              timeUntilNext = nextPumpTime - millis();
            } else {
              timeUntilNext = 0;  // Уже пора включать
            }
            
            int minutesUntilNext = (timeUntilNext / 1000) / 60;
            int secondsUntilNext = (timeUntilNext / 1000) % 60;
            
            // Защита от переполнения и больших значений
            if (minutesUntilNext > 999) minutesUntilNext = 0;
            if (secondsUntilNext > 59) secondsUntilNext = 0;
            
            tft.setCursor(0, 66);
            tft.print(utf8rus("Через "));
            if (minutesUntilNext > 0) {
              tft.print(minutesUntilNext);
              tft.print(utf8rus("м "));
            }
            tft.print(secondsUntilNext);
            tft.println(utf8rus("с"));
          }
        } else if (!water) {
          tft.setCursor(0, 66);
          tft.println(utf8rus("Нет воды!"));
        } else {
          tft.setCursor(0, 66);
          tft.println(utf8rus("---"));
        }
        tft.setCursor(0, 84); 
        tft.print(utf8rus("Питание:"));
        tft.println(bbp_state ? utf8rus("Сеть") : utf8rus("Аккум"));
        timer_displey = millis();
        sch = sch + 1;
      }
      break;
      case 4:
      if(millis() - timer_displey >= 5000){
        tft.fillScreen(ST77XX_BLACK); 
        tft.setTextSize(2);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_CYAN);
        tft.println(utf8rus("  Освещение"));
        
        // Основная информация
        tft.setTextSize(2);
        tft.setTextColor(ST77XX_GREEN);
        tft.setCursor(0, 30);
        tft.print(utf8rus("Режим:"));
        tft.println(autoLightEnabled ? utf8rus("Авто") : utf8rus("Ручной"));
        
        // Статус и время
        tft.setCursor(0, 48);
        if (lightEnabled) {
          tft.println(utf8rus("Статус:ВКЛ"));
          if (autoLightEnabled) {
            // Показываем время до выключения
            int currentTime = Hour * 60 + Minute;
            int endTime = lightOffHour * 60 + lightOffMinute;
            int minutesLeft = endTime - currentTime;
            
            if (minutesLeft < 0) minutesLeft += 1440; // Если перешли через полночь
            
            tft.setCursor(0, 66);
            tft.print(utf8rus("До "));
            tft.print(lightOffHour);
            tft.print(":");
            if(lightOffMinute < 10) tft.print("0");
            tft.println(lightOffMinute);
            tft.println(utf8rus("Осталось " + String(minutesLeft) + " мин"));
          }
        } else {
          tft.println(utf8rus("Статус:ВЫКЛ"));
          if (autoLightEnabled) {
            // Показываем время до включения
            int currentTime = Hour * 60 + Minute;
            int startTime = lightOnHour * 60 + lightOnMinute;
            int minutesLeft = startTime - currentTime;
            
            if (minutesLeft < 0) minutesLeft += 1440; // Если перешли через полночь
            
            tft.setCursor(0, 66);
            tft.print(utf8rus("Вкл в "));
            tft.print(lightOnHour);
            tft.print(":");
            if(lightOnMinute < 10) tft.print("0");
            tft.println(lightOnMinute);
            tft.println(utf8rus("Осталось " + String(minutesLeft) + " мин"));
          }
        }
        
        timer_displey = millis();
        sch = 0;
      }
      break;
  }
}


void checkECAlerts() {
  static uint32_t last_EC_check = 0;
  static bool EC1_sent = false, EC2_sent = true;
  // Проверяем воду каждые 30 секунд (можно изменить)
  if (millis() - last_EC_check >= 5000) {
    last_EC_check = millis();
    
    if ((mineral > 2.5 || mineral < 1) && !EC1_sent) {
      sendWaterAlertToAll(String("⚠️ВНИМАНИЕ!⚠️\nEC вышел за допустимые пределы.") + String("\n") + String("EC: ") + String(mineral,1) + String("мСм/см"));
      EC1_sent = true;
      EC2_sent = false;
    }
    
    else if ((mineral < 2.5 && mineral > 1) && !EC2_sent) {
      sendWaterAlertToAll(String("✅Восстановление!✅\nEC в норме.") + String("\n") + String("EC: ") + String(mineral,1) + String("мСм/см"));
      EC2_sent = true;
      EC1_sent = false;
    }
  }
}


void checkTEMPAlerts() {
  static uint32_t last_TEMP_check = 0;
  static bool TEMP1_sent = false, TEMP2_sent = true;
  // Проверяем воду каждые 30 секунд (можно изменить)
  if (millis() - last_TEMP_check >= 5000) {
    last_TEMP_check = millis();
    
    if ((t_water > 25 || t_water < 18) && !TEMP1_sent) {
      sendWaterAlertToAll(String("⚠️ВНИМАНИЕ!⚠️\nТемпература корневой зоны вышла за безопасные пределы.") + String("\n") + String("Температура: ") + String(t_water) + String("°C"));
      TEMP1_sent = true;
      TEMP2_sent = false;
    }
    
    else if ((t_water < 25 && t_water > 18) && !TEMP2_sent) {
      sendWaterAlertToAll(String("✅Восстановление!✅\nТемпература корневой зоны в норме.") + String("\n") + String("Температура: ") + String(t_water) + String("°C"));
      TEMP2_sent = true;
      TEMP1_sent = false;
    }
  }
}


void checkWaterAlerts() {
  static uint32_t last_water_check = 0;
  
  // Проверяем воду каждые 30 секунд (можно изменить)
  if (millis() - last_water_check >= 5000) {
    last_water_check = millis();
    
    bool current_water_state = water; 
    
    if (!current_water_state && last_water_state && !water_alert_sent) {
      sendWaterAlertToAll("⚠️ВНИМАНИЕ!⚠️\n Раствор скоро ЗАКОНЧИТСЯ!\n Необходимо пополнить бак.");
      water_alert_sent = true;
      water_alert_timer = millis();
      water_recovered_sent = false;
    }
    
    else if (current_water_state && !last_water_state && !water_recovered_sent) {
      sendWaterAlertToAll("✅Восстановление!✅\nБак с раствором пополнен.");
      water_recovered_sent = true;
      water_alert_sent = false;
    }
    
    else if (!current_water_state && water_alert_sent && (millis() - water_alert_timer >= 900000)) { // 15 минут = 900000 мс
      sendWaterAlertToAll("🔔НАПОМИНАНИЕ!🔔\n Раствор скоро ЗАКОНЧИТСЯ!\n Пополните бак.");
      water_alert_timer = millis();
    }
    
    last_water_state = current_water_state;
  }
}

// Функция для отправки оповещения всем пользователям
void sendBbpAlertToAll(String message) {
  Serial.println("Отправка BPP оповещения: " + message);
  
  for (int i = 0; i < 15; i++) {
    if (Chat_ID_DATA[i] != "" && Chat_ID_DATA[i] != "0") {
      bot.sendMessage(message, Chat_ID_DATA[i]);
      delay(200); // Небольшая задержка между отправками
    }
  }
}

// Функция для проверки и отправки оповещений о состоянии БПП
void checkBbpAlerts() {
  static bool last_bbp_state = true; // Предполагаем, что изначально сеть есть
  static bool bbp_alert_sent = false; // Флаг отправки оповещения о переходе на аккумулятор
  static bool bbp_recovered_sent = false; // Флаг отправки оповещения о восстановлении сети
  static uint32_t bbp_alert_timer = 0; // Таймер для повторных оповещений
  
  // Проверяем каждые 5 секунд (можно изменить интервал)
  static uint32_t last_bbp_check = 0;
  if (millis() - last_bbp_check >= 5000) {
    last_bbp_check = millis();
    
    bool current_bbp_state = bbp_state; // true = сеть 220В, false = аккумулятор
    
    // ПЕРЕХОД НА АККУМУЛЯТОР (потеря сети)
    if (!current_bbp_state && last_bbp_state && !bbp_alert_sent) {
      String alertMsg = "⚠️ВНИМАНИЕ!⚠️\n";
      alertMsg += "Произошло ⚡️отключение сети 220В⚡️!\n";
      alertMsg += "Система перешла на питание от 🔋аккумулятора🔋.\n";
      alertMsg += "Рекомендуется проверить электроснабжение. " + String(analogRead(BBP));
      
      sendBbpAlertToAll(alertMsg);
      bbp_alert_sent = true;
      bbp_alert_timer = millis();
      bbp_recovered_sent = false;
      
      Serial.println("Оповещение: переход на аккумулятор");
    }
    
    // ВОССТАНОВЛЕНИЕ СЕТИ 220В
    else if (current_bbp_state && !last_bbp_state && !bbp_recovered_sent) {
      String alertMsg = "✅ВОССТАНОВЛЕНИЕ!✅\n";
      alertMsg += "Сеть ⚡️220В восстановлена⚡️!\n";
      alertMsg += "Система перешла на основное питание. " + String(analogRead(BBP));
      
      sendBbpAlertToAll(alertMsg);
      bbp_recovered_sent = true;
      bbp_alert_sent = false;
      
      Serial.println("Оповещение: восстановление сети 220В");
    }
    
    // ПОВТОРНОЕ ОПОВЕЩЕНИЕ ЕСЛИ ВСЁ ЕЩЁ НА АККУМУЛЯТОРЕ (каждые 30 минут)
    else if (!current_bbp_state && bbp_alert_sent && (millis() - bbp_alert_timer >= 1800000UL)) {
      String alertMsg = "🔔НАПОМИНАНИЕ!🔔\n";
      alertMsg += "Система всё ещё работает от *аккумулятора*!\n";
      alertMsg += "Прошло уже 30 минут.\n";
      alertMsg += "⚡️Проверьте электроснабжение!⚡️ " + String(analogRead(BBP));
      
      sendBbpAlertToAll(alertMsg);
      bbp_alert_timer = millis();
      
      Serial.println("Повторное оповещение: всё ещё на аккумуляторе");
    }
    
    // Обновляем предыдущее состояние
    last_bbp_state = current_bbp_state;
  }
}

// Функция отправки оповещений всем пользователям
void sendWaterAlertToAll(String message) {
  Serial.println("Отправка оповещения: " + message);
  
  for (int i = 0; i < 15; i++) {
    if (Chat_ID_DATA[i] != "" && Chat_ID_DATA[i] != "0") {
      bot.sendMessage(message, Chat_ID_DATA[i]);
      delay(200); 
    }
  }
}

void sendToThingSpeak() {
  
  Serial.println("Отправка данных в ThingSpeak...");
  
  ThingSpeak.setField(1, t);           // Температура воздуха
  ThingSpeak.setField(2, h);           // Влажность
  ThingSpeak.setField(3, p);           // Давление
  ThingSpeak.setField(4, t_water);     // Температура воды
  ThingSpeak.setField(5, mineral);     // Минерализация
  ThingSpeak.setField(6, water ? 1 : 0); // Наличие воды (1=есть, 0=нет)
  ThingSpeak.setField(7, bbp_state ? 1 : 0); // Питание (1=сеть, 0=аккумулятор)
  ThingSpeak.setField(8, lightEnabled ? 1 : 0); // Освещение (1=вкл, 0=выкл)
  
  // Отправляем данные
  int httpCode = ThingSpeak.writeFields(2, lp.thingspeakAPIKey);
  
  if (httpCode == 200) {
    Serial.println("Данные успешно отправлены в ThingSpeak");
  } else {
    Serial.print("Ошибка отправки в ThingSpeak. Код: ");
    Serial.println(httpCode);
  }
  thingspeakEnabled = true;
}

float readTemperature() {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  oneWire.reset_search();
  // Поиск подключенных датчиков DS18B20 на шине OneWire
  if (!oneWire.search(addr)) {
    Serial.println("Датчики DS18B20 не найдены.");
    // Сбрасываем поиск и возвращаем "NaN" (не число) в случае ошибки
    oneWire.reset_search();
    return -40;
  }
 
  // Проверка целостности адреса датчика с помощью CRC
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC не совпадает!");
    return -50;
  }
 
  // Определение типа датчика на основе первого байта адреса
  switch (addr[0]) {
    case 0x10:
      type_s = 1; // DS18S20 или DS1822
      break;
    case 0x28:
    case 0x22:
      type_s = 0; // DS18B20
      break;
    default:
      Serial.println("Неизвестный тип датчика.");
      return -60;
  }
 
  // Сбрасываем шину и выбираем адрес конкретного датчика
  oneWire.reset();
  oneWire.select(addr);
  
  // Запускаем измерение температуры на датчике
  oneWire.write(0x44); // 0x44 - команда начать измерение
 
  // Ожидание завершения измерения (время зависит от разрешения)
  delay(1000);
 
  // Сбрасываем шину и выбираем адрес датчика для чтения данных
  present = oneWire.reset();
  oneWire.select(addr);
  oneWire.write(0xBE); // 0xBE - команда чтения данных
 
  // Считываем 9 байт данных температуры и CRC
  for (i = 0; i < 9; i++) {
    data[i] = oneWire.read();
  }
 
  // Преобразование считанных данных в температуру
  int16_t raw = (data[1] << 8) | data[0];
  float celsius = 0.0;
 
  // Применяем разрешение и коррекцию для разных типов датчиков
  if (type_s == 1) {
    raw = raw << 3; // Увеличиваем разрешение до 12 бит
    if (data[7] == 0x10) {
      // Температура с высоким разрешением (DS18S20)
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7; // Разрешение 9 бит, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // Разрешение 10 бит, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // Разрешение 11 бит, 375 ms
  }
  celsius = (float)raw / 16.0; // Преобразуем вещественное значение температуры и возвращаем
 
  return celsius;
}
float getMineral() {
  static float averageVoltage = 0;
  static float EC_value = 0;  // EC в мСм/см
  
  // Константы для расчета EC
  const float Ka = 1000.0f;   // Множитель степенной функции
  const float Kb = -5.0f;     // Степень степенной функции
  const float Kt = 0.02f;     // Температурный коэффициент
  const float Kf = 0.85f;     // Коэффициент передачи ФВЧ+ФНЧ модуля
  const float T_ref = 25.0f;  // Опорная температура в °C
    
    // Получаем среднее напряжение на выходе модуля (в Вольтах)
    averageVoltage = analogRead(TDS) * (float)VREF / 4095.0;
    
    // Используем температуру воды для коррекции
    float current_temperature = t_water;  // Используем измеренную температуру воды
    
    // Рассчитываем удельную электропроводность (мСм/см)
    const float Vccm = 3.3f;
    float Vm = averageVoltage;
    
    // Проверка на переполнение при возведении в отрицательную степень
    float base = (Vccm - Kf * Vm) / 2.0f;
    if (base <= 0) {
      base = 0.0001f;
    }
    
    // Расчет удельной электропроводности S (мСм/см)
    float S = Ka * pow(base, Kb) / 1000.0f;
    
    // Приводим EC к опорной температуре 25°C
    EC_value = S / (1.0f + Kt * (current_temperature - T_ref));
    
    // Защита от некорректных значений
    if (EC_value < 0) EC_value = 0;
    if (EC_value > 5000) EC_value = 5000;  // Максимальное разумное значение
    
    // Выводим результат
    Serial.print("Напряжение: ");
    Serial.print(averageVoltage, 3);
    Serial.print(" V, Температура: ");
    Serial.print(current_temperature, 1);
    Serial.print(" °C, EC: ");
    Serial.print(EC_value, 1);
    Serial.println(" мСм/см");
  
    return float(EC_value);
}
