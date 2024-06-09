/* Includes Made Before Definitions:
================================================================================*/
#include <Arduino.h>
#include "PinDefinitionsAndMore.h"
#include <IRremote.hpp>
#include <WiFiManager.h>
#include <iostream>
#include <Preferences.h> // Include Preferences library

/* Defines:
================================================================================*/
/* Env Defines:
============================================================================*/
#include "env.h"

/* Universal Controller Defines:
============================================================================*/
#define RAW_BUFFER_LENGTH 750
#define MARK_EXCESS_MICROS 20 // 20 is recommended for the cheap VS1838 modules

/* Board Defines:
============================================================================*/
#define pinLED 27
#define RED_LED 23
#define GREEN_LED 22

/* IR Pin Definitions:
============================================================================*/
#define IR_RECEIVE_PIN 15 // Replace with the actual pin connected to the IR receiver
#define IR_SEND_PIN 14    // Replace with the actual pin connected to the IR sender

/* Include Made After the Declarations:
================================================================================*/
#include <Blynk.h>
#include <BlynkSimpleEsp32.h>

/* Structs & Variables:
================================================================================*/
/* Structs:
============================================================================*/
/**
 * @brief Struct to store IR data and raw codes
 */
struct storedIRDataStruct {
    IRData receivedIRData;  // Extensions for sendRaw
    uint8_t rawCode[RAW_BUFFER_LENGTH]; // The durations if raw
    uint16_t rawCodeLength; // The length of the code
} sStoredIRData[10]; // Array to store IR data for 10 commands

/* Global Variables:
================================================================================*/
int DELAY_BETWEEN_REPEAT = 50;
int DEFAULT_NUMBER_OF_REPEATS_TO_SEND = 1;
int estadoAnt = 0; // Variable to control the change of operation mode
int commandPosition = -1; // Command position for storing and sending
String setMode = "Any"; // Mode of the system
String statusModeSet = "Any"; // Status mode to display on Blynk
int estadoAtual = 0; // Current state for recording or sending
int blynkWorking = 0; // To control working LED
int blynkError = 0; // To control error LED
unsigned long previousMillis = 0;
const long interval = 1000; // Interval at which to blink (milliseconds)
int shouldRestart = 0; // Flag to determine if the board should restart

Preferences preferences; // Create a Preferences object

/* Prototypes:
================================================================================*/
void checkIR();
void storeCode(IRData *aIRReceivedData, int index);
void sendCode(storedIRDataStruct *aIRDataToSend);
void saveIRCode(int index);
void loadIRCodes();
void wifiConnection();
void blynkConnection();
void gettingStatusMode();
void disableReader();
void disableSender();
void resettingCommandPosition();
void setErrorOn();
void setErrorOff();
void setWorkingOn();
void setWorkingOff();
void blynkWorkingLed();
void blynkErrorLed();
void restartBoard();

/* Setup Function:
================================================================================*/
void setup() {
    /* Setting Led Builtin Feedback:
    ==========================================================================*/
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    /* Setting Serial Monitor Frequency:
    ==========================================================================*/
    Serial.begin(115200);

    /* Calling Essential Methods for First Load:
    ==========================================================================*/
    wifiConnection();
    blynkConnection();

    setWorkingOn();

    Serial.println(IR_SEND_PIN);    // Shows in which pin the sender is set to
    Serial.println(IR_RECEIVE_PIN); // Shows in which pin the receiver is set to

    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
    IrSender.begin();

    preferences.begin("IRCodes", false); // Initialize preferences with a namespace
    loadIRCodes(); // Load stored IR codes from NVS

    /* Printing Instructions for Serial Monitor Use:
    ==========================================================================*/
    Serial.println(F("Para gravar digite '#' seguido da posicao entre 0 e 9"));
    Serial.println(F("Para enviar digite o numero entre 0 e 9 de um codigo ja gravado"));
}

/* Loop Function:
================================================================================*/
void loop() {
    Blynk.run();
    checkIR();
    blynkWorkingLed();
    blynkErrorLed();
    restartBoard();
}

/* Check IR Status Function:
================================================================================*/
/**
 * @brief Function to check the IR receiver status and handle different states.
 */
void checkIR() {
    // Start recording command
    if (estadoAtual != estadoAnt && estadoAtual == 1) {
        IrReceiver.start();
    }

    // Send command
    if (estadoAtual == 2 && commandPosition >= 0 && commandPosition < 10) {
        IrReceiver.stop();
        Serial.println(F("Enviando..."));
        sendCode(&sStoredIRData[commandPosition]);
        commandPosition = -1; // Reset commandPosition to avoid repeated sending
    }

    // Record command
    if (estadoAtual == 1 && IrReceiver.available() && commandPosition >= 0 && commandPosition < 10) {
        storeCode(IrReceiver.read(), commandPosition);
        IrReceiver.resume(); // Resume receiver
    }

    estadoAnt = estadoAtual;
}

/* Store Code Function:
================================================================================*/
/**
 * @brief Function to store the received IR code in the given index.
 * @param aIRReceivedData Pointer to the received IR data.
 * @param index Index to store the received IR data.
 */
void storeCode(IRData *aIRReceivedData, int index) {
    if (aIRReceivedData->flags & IRDATA_FLAGS_IS_REPEAT) {
        Serial.println(F("Ignore repeat"));
        return;
    }
    if (aIRReceivedData->flags & IRDATA_FLAGS_IS_AUTO_REPEAT) {
        Serial.println(F("Ignore autorepeat"));
        return;
    }
    if (aIRReceivedData->flags & IRDATA_FLAGS_PARITY_FAILED) {
        Serial.println(F("Ignore parity error"));
        return;
    }

    // Copy decoded data
    sStoredIRData[index].receivedIRData = *aIRReceivedData;

    if (sStoredIRData[index].receivedIRData.protocol == UNKNOWN) {
        Serial.print(F("Received unknown code and store "));
        Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
        Serial.println(F(" timing entries as raw "));
        IrReceiver.printIRResultRawFormatted(&Serial, true); // Output the results in RAW format
        sStoredIRData[index].rawCodeLength = IrReceiver.decodedIRData.rawDataPtr->rawlen - 1;
        // Store the current raw data in a dedicated array for later usage
        IrReceiver.compensateAndStoreIRResultInArray(sStoredIRData[index].rawCode);
    } else {
        IrReceiver.printIRResultShort(&Serial);
        IrReceiver.printIRSendUsage(&Serial);
        sStoredIRData[index].receivedIRData.flags = 0; // Clear flags, especially repeat, for later sending
        Serial.println();
    }

    saveIRCode(index); // Save the stored IR code to NVS
}

/* Save IR Code Function:
================================================================================*/
/**
 * @brief Function to save the stored IR code to NVS.
 * @param index Index of the stored IR data.
 */
void saveIRCode(int index) {
    char key[10];
    sprintf(key, "IRCode%d", index);

    String lengthKey = String(key) + "_len";
    preferences.putUShort(lengthKey.c_str(), sStoredIRData[index].rawCodeLength);

    for (int i = 0; i < sStoredIRData[index].rawCodeLength; i++) {
        String dataKey = String(key) + String(i);
        preferences.putUChar(dataKey.c_str(), sStoredIRData[index].rawCode[i]);
    }
}


/* Load IR Codes Function:
================================================================================*/
/**
 * @brief Function to load stored IR codes from NVS.
 */
void loadIRCodes() {
    for (int index = 0; index < 10; index++) {
        char key[10];
        sprintf(key, "IRCode%d", index);

        String lengthKey = String(key) + "_len";
        sStoredIRData[index].rawCodeLength = preferences.getUShort(lengthKey.c_str(), 0);

        for (int i = 0; i < sStoredIRData[index].rawCodeLength; i++) {
            String dataKey = String(key) + String(i);
            sStoredIRData[index].rawCode[i] = preferences.getUChar(dataKey.c_str(), 0);
        }
    }
}

/* Send Code Function:
================================================================================*/
/**
 * @brief Function to send the stored IR code.
 * @param aIRDataToSend Pointer to the stored IR data to send.
 */
void sendCode(storedIRDataStruct *aIRDataToSend) {
    if (aIRDataToSend->receivedIRData.protocol == UNKNOWN) {
        // Assume 38 KHz
        IrSender.sendRaw(aIRDataToSend->rawCode, aIRDataToSend->rawCodeLength, 38);
        Serial.print(F("Sent raw "));
        Serial.print(aIRDataToSend->rawCodeLength);
        Serial.println(F(" marks or spaces"));
    } else {
        // Use the write function, which switches for different protocols
        IrSender.write(&aIRDataToSend->receivedIRData, DEFAULT_NUMBER_OF_REPEATS_TO_SEND);
        Serial.print(F("Sent: "));
        printIRResultShort(&Serial, &aIRDataToSend->receivedIRData, false);
    }
}

/* Wi-Fi Connection:
================================================================================*/
/**
 * @brief Function to manage Wi-Fi connection using WiFiManager.
 */
void wifiConnection() {
    WiFiManager wm;
    bool res;
    res = wm.autoConnect("esp32_reset","bettergoodluck");

    if (!res) {
        Serial.println("Failed to connect");
        setErrorOn();
        statusModeSet = "Error"; // Set status mode to Error
        blynkError = 1;
        gettingStatusMode();
        ESP.restart();
    } else {
        blynkError = 0;
        // If you get here, you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }
}

/* Blynk Functions:
================================================================================*/
/* Blynk Connection:
============================================================================*/
/**
 * @brief Function to connect to Blynk.
 */
void blynkConnection() {
    Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);
    if (!Blynk.connected()) {
        Serial.println("Failed to connect to Blynk");
        setErrorOn();
        statusModeSet = "Error"; // Set status mode to Error
        blynkError = 1;
        gettingStatusMode();
        ESP.restart();
    }
}

/* Reading Value to Set Reader Mode On:
============================================================================*/
/**
 * @brief Function to handle Blynk virtual write for setting reader mode.
 */
BLYNK_WRITE(V0) {
    int v0_value = param.asInt();

    if (v0_value == 1 && setMode != "Sender") {
        setMode = "Reader";
        statusModeSet = "Recording";
        estadoAtual = 1;
        Serial.println("Reader mode enabled via Blynk");
        blynkWorking = 1;
    } else {
        disableReader();
        resettingCommandPosition();
    }
    gettingStatusMode();
}

/* Reading Value to Set Sender Mode On:
============================================================================*/
/**
 * @brief Function to handle Blynk virtual write for setting sender mode.
 */
BLYNK_WRITE(V1) {
    int v1_value = param.asInt();

    if (v1_value == 1 && setMode != "Reader") {
        setMode = "Sender";
        statusModeSet = "Sender Mode";
        estadoAtual = 2;
        Serial.println("Sender mode enabled via Blynk");
        blynkWorking = 1;
    } else {
        disableSender();
        resettingCommandPosition();
    }
    gettingStatusMode();
}

/* Getting Mode Status to Show on App:
============================================================================*/
/**
 * @brief Function to update Blynk with the current status mode.
 */
void gettingStatusMode() {
    Serial.println(statusModeSet);
    Blynk.virtualWrite(V2, statusModeSet);
}

/* Disabling Reader Switcher:
============================================================================*/
/**
 * @brief Function to disable reader mode.
 */
void disableReader() {
    Serial.println("Disabling Reader Mode Switcher");
    Blynk.virtualWrite(V0, 0);
    setMode = "Any";
    statusModeSet = "Any";
    estadoAtual = 0;
    blynkWorking = 0;
    IrReceiver.stop();
}

/* Disabling Sender Switcher:
============================================================================*/
/**
 * @brief Function to disable sender mode.
 */
void disableSender() {
    Serial.println("Disabling Sender Mode Switcher");
    Blynk.virtualWrite(V1, 0);
    setMode = "Any";
    statusModeSet = "Any";
    estadoAtual = 0;
    blynkWorking = 0;
}

/* Getting Command Position:
============================================================================*/
/**
 * @brief Function to handle Blynk virtual write for setting command position.
 */
BLYNK_WRITE(V3) {
    commandPosition = param.asInt();
    Serial.println(commandPosition);
}

/* Setting Command Position:
============================================================================*/
/**
 * @brief Function to reset command position in Blynk.
 */
void resettingCommandPosition() {
    Serial.println("Setting Command Position to -1 in Blynk Cloud!");
    commandPosition = -1;
    Blynk.virtualWrite(V3, commandPosition);
}

/* Set Error Led (RED) On:
============================================================================*/
/**
 * @brief Function to turn on the error LED.
 */
void setErrorOn() {
    setWorkingOff(); // When setting red, the green should stop.
    digitalWrite(RED_LED, HIGH);
    delay(50);
}

/* Set Error Led (RED) Off:
============================================================================*/
/**
 * @brief Function to turn off the error LED.
 */
void setErrorOff() {
    digitalWrite(RED_LED, LOW);
    delay(1000);      
}

/* Set Normal Working Status Led (Green) On:
============================================================================*/
/**
 * @brief Function to turn on the working LED.
 */
void setWorkingOn() {
    setErrorOff(); // When setting green, the red should stop.
    digitalWrite(GREEN_LED, HIGH);
    delay(1000);
}

/* Set Normal Working Status Led (Green) Off:
============================================================================*/
/**
 * @brief Function to turn off the working LED.
 */
void setWorkingOff() {
    digitalWrite(GREEN_LED, LOW);
    delay(1000);
}

/* Blink Working Led:
============================================================================*/
/**
 * @brief Function to blink the working LED based on the interval.
 */
void blynkWorkingLed() {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();

    if (blynkWorking == 1 && currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        digitalWrite(GREEN_LED, !digitalRead(GREEN_LED));
    }
}

/* Blink Error Led:
============================================================================*/
/**
 * @brief Function to blink the error LED based on the interval.
 */
void blynkErrorLed() {
    static unsigned long previousMillis = 0;
    unsigned long currentMillis = millis();

    if (blynkError == 1 && currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        digitalWrite(RED_LED, !digitalRead(RED_LED));
    }
}

/* Restart ESP32:
============================================================================*/
/**
 * @brief Function to restart the ESP32 if the restart flag is set.
 */
void restartBoard() {
    if (shouldRestart == 1) {
        Blynk.virtualWrite(V4, 0);
        delay(1000);
        ESP.restart();
    }
}

/* Getting Restart Command:
============================================================================*/
/**
 * @brief Function to handle Blynk virtual write for setting the restart flag.
 */
BLYNK_WRITE(V4) {
    shouldRestart = param.asInt();
    Serial.println(shouldRestart);
}