#include <WiFi.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "YK";
const char* password = "abcd1234";

WiFiServer server(80);

// Servo objects
Servo servoX;
Servo servoY;

// Servo pins
const int servoXPin = 18;
const int servoYPin = 19;
const int laserPin  = 21;

// Camera center (pixels)
float xCenterPixel = 320;
float yCenterPixel = 240;

// Calibration
float baseDistanceX = 15.0;  // max horizontal range (cm)
float baseDistanceY = 33.0;  // max vertical range (cm)
float cmPerPixel   = 0.05;  // Scale: how many cm per pixel in camera

// Servo center positions (degrees)
float xServoCenter = 0.0;
float yServoCenter = 0.0;

// Galvo system height
float galvoHeight = 25.0;

// Motor Driver Pins
const int RPWM_L = 25; 
const int LPWM_L = 26; 
const int RPWM_R = 27; 
const int LPWM_R = 14; 

// Motor speed (0-125)
int maxMotorSpeed = 125;

void setup() {
  Serial.begin(115200);

  // Attach servos
  servoX.attach(servoXPin);
  servoY.attach(servoYPin);

  // Laser pin
  pinMode(laserPin, OUTPUT);
  digitalWrite(laserPin, HIGH);

  // Motor pins
  pinMode(RPWM_L, OUTPUT);
  pinMode(LPWM_L, OUTPUT);
  pinMode(RPWM_R, OUTPUT);
  pinMode(LPWM_R, OUTPUT);

  // Move servos to center
  servoX.write(xServoCenter);
  servoY.write(yServoCenter);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  Serial.println("Galvo servo system and motors ready, WebServer started");
}

// Move servos to given pixel coordinates and set motor speed
void moveGalvo(int xPixel, int yPixel) {
  float x_cm = (xPixel - xCenterPixel) * cmPerPixel;
  float y_cm = (yCenterPixel - yPixel) * cmPerPixel;

  // Limit lengths
  x_cm = constrain(x_cm, -15, 15);
  y_cm = constrain(y_cm, 0, 33);

  // Convert cm offset to angles
  float xAngle = atan(x_cm / galvoHeight) * 180.0 / PI;
  float yAngle = atan(y_cm / galvoHeight) * 180.0 / PI;

  // Limit angles
  xAngle = constrain(xAngle, -5, 5);
  yAngle = constrain(yAngle, 0, 10);

  float finalX = xServoCenter + xAngle;
  float finalY = yServoCenter + yAngle;

  servoX.write(finalX);
  servoY.write(finalY);

  // Calculate motor speed proportional to distance from center
  int motorSpeed = map(abs(x_cm) + abs(y_cm), 0, 48, 0, maxMotorSpeed); // 48 = max reachable distance
  motorSpeed = constrain(motorSpeed, 0, maxMotorSpeed);

  Serial.print("Angles: "); Serial.print(finalX); Serial.print(", "); Serial.print(finalY);
  Serial.print("  | Motor Speed: "); Serial.println(motorSpeed);

  // Move motors forward with calculated speed as default
  analogWrite(LPWM_L, motorSpeed); analogWrite(RPWM_L, 0);
  analogWrite(LPWM_R, motorSpeed); analogWrite(RPWM_R, 0);
}

// Stop all motors
void stopMotors() {
  analogWrite(RPWM_L, 0);
  analogWrite(LPWM_L, 0);
  analogWrite(RPWM_R, 0);
  analogWrite(LPWM_R, 0);
  delay(100);
}

// Motor control helper functions
void moveForward(int speed) {
  analogWrite(LPWM_L, speed); analogWrite(RPWM_L, 0);
  analogWrite(LPWM_R, speed); analogWrite(RPWM_R, 0);
}

void moveBackward(int speed) {
  analogWrite(LPWM_L, 0); analogWrite(RPWM_L, speed);
  analogWrite(LPWM_R, 0); analogWrite(RPWM_R, speed);
}

void turnLeft(int speed) {
  analogWrite(LPWM_L, 0); analogWrite(RPWM_L, speed);
  analogWrite(LPWM_R, speed); analogWrite(RPWM_R, 0);
}

void turnRight(int speed) {
  analogWrite(LPWM_L, speed); analogWrite(RPWM_L, 0);
  analogWrite(LPWM_R, 0); analogWrite(RPWM_R, speed);
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    int xIndex = request.indexOf("x=");
    int yIndex = request.indexOf("y=");
    int actionIndex = request.indexOf("action=");

    if (xIndex != -1 && yIndex != -1) {
      int xEnd = request.indexOf("&", xIndex);
      if (xEnd == -1) xEnd = request.indexOf(' ', xIndex);
      int yEnd = request.indexOf(' ', yIndex);

      int xPixel = request.substring(xIndex + 2, xEnd).toInt();
      int yPixel = request.substring(yIndex + 2, yEnd).toInt();

      Serial.print("Received Pixels - X: "); Serial.print(xPixel);
      Serial.print("  Y: "); Serial.println(yPixel);

      moveGalvo(xPixel, yPixel);
    }

    if (actionIndex != -1) {
      int actionEnd = request.indexOf(' ', actionIndex);
      String action = request.substring(actionIndex + 7, actionEnd);

      Serial.print("Action: "); Serial.println(action);

      int speed = maxMotorSpeed; // default speed
      if (action == "forward") moveForward(speed);
      else if (action == "backward") moveBackward(speed);
      else if (action == "left") turnLeft(speed);
      else if (action == "right") turnRight(speed);
      else stopMotors();
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    client.println("OK");
    client.stop();
  }
}