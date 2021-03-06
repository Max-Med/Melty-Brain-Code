#include "avr/interrupt.h"

unsigned int rc[6];
float timer_period, previousAngle, time, currentAngle;
unsigned int T, T_reset, T_prev, T_offset, T_min, T_max;
unsigned short T_pointer=0;
unsigned int tics_before_reset;
int previousRc[5], throttle;
unsigned long previousMillis;  
float radius = 0.03;
float fraction= 0.000;
 
void setup()
{ 
  Serial.begin(115200);
  pinMode (2, OUTPUT);
  digitalWrite(2, HIGH);
  for (int i=8; i<14; i++) {
    pinMode(i,INPUT);
  }
  
  // Timer1 setup
  TCCR1A = 0b00000000;          
  TCCR1B = 0b00000011;        // Start timer1 with prescaler 8
  timer_period = 0.000004;    // One tick takes 4 us
  PORTB|=0b00111111;          // Turn on pullup resistors
  PCMSK0=0b00111111;          // Pin change interrupt 0 bit mask: we interrupt on change on pins PCINT0-5 (arduino nano pins 8 - 13)
  PCICR=0b00000001;           // Enable pin change interrupt 0
  sei();                      // Global interrupt enable  
  TCNT1 = 0;                  // Reset timer1

  T_min = (unsigned int) (0.0005 / timer_period); // shorter than 0.5ms pwm signal is ignored
  T_max = (unsigned int) (0.003 / timer_period); // longer than 3ms pwm signal indicates frame end
  rc[0]=375;                  // Init rc with sane values
  rc[1]=375;
  rc[2]=375;
  rc[3]=375;
  rc[4]=375;
  rc[5]=375;

  tics_before_reset = (unsigned int) (0.10/timer_period); // 0.1 sec control loop (25000 tics)
  
  Serial.println("Read RC receiver test");
  pinMode(6, OUTPUT);
  delay(1000);
}

void loop() //Main Loop
{
  while (TCNT1 < tics_before_reset) ; // Wait until start of new frame
 
  cli(); // No interrupts allowed when modifying timer1
  T_reset = TCNT1;
  TCNT1 = 0;
  T_offset = T_reset - T_prev;
  T_prev=0;
  sei(); // Reactivate interrupts. 
  
    
  float heading = headingSet(rc[0], rc[2]);  //should return angle in radians of vectore (rc[1],rc[3]) 
  long translationalSpeed = speedSet(rc[0], rc[2]);  //should return magnitude of vector (rc[1],rc[3])
  if (rc[5]< previousRc[5] + 10 && rc[5] > previousRc[5] - 10) {
     throttle= map(rc[5], 268, 483, 0, 255);  //should map rc[6] between 0 and 255
   }
  previousRc[5]=rc[5]; 
  time = millis()- previousMillis;
  previousMillis= millis();
  currentAngle  = currentAngleSet(analogRead(A0), rc[4], previousAngle, time); 
  previousAngle = currentAngle;
  if (currentAngle > 5.4978 || currentAngle < 0.7854){
    digitalWrite(2, HIGH);
  }
  else{
    digitalWrite(2, LOW);
  }
  
  
  controlMotor(throttle,heading, currentAngle, translationalSpeed, 3);
  
  controlMotor(throttle,heading, currentAngle, translationalSpeed, 5);  
  controlMotor(throttle,heading, currentAngle, translationalSpeed, 6);
  
  Serial.print(heading*57.300);
  Serial.print("\t");
  Serial.print(translationalSpeed);
  Serial.print("\t");  
  Serial.print(currentAngle*57.300);
  Serial.print("\t");  
  Serial.print(map(analogRead(A0), 0, 1000, -4934, 9707));
  Serial.print("\t");  
  Serial.print(time);  
  Serial.print("\t");  
  Serial.println(throttle); 
  /*if( throttle > 20){
    //analogWrite (6, throttle);
    analogWrite (5, throttle);
    analogWrite (3, throttle);
  }
  else {
    //analogWrite (6, 0);
    analogWrite (5, 0);
    analogWrite (3, 0);
  } */
}


ISR(PCINT0_vect) {
  T = TCNT1 - T_prev + T_offset;
  if (T > T_min) {
    T_prev = TCNT1;
    T_offset=0;
    if (T > T_max) {
      T_pointer = 0;
    } 
    else {
      rc[T_pointer] = T;
      T_pointer++;
      if (T_pointer > 5) T_pointer--; // overflow protection (glitches can happen...)
    }
  }
}
 
float headingSet(int x, int y){ 
  float angle;
  float trueX = map(x, 270, 480, -1000, 1000);
  float trueY = map(y, 270, 480, -1000, 1000);
  float YoverX = trueY/trueX ;
  float fraction = abs (YoverX);
  if (trueY >= 0 && trueX >= 0) {
    angle= 4.71239 + atan(fraction);
  }
  else if (trueY <= 0 && trueX >= 0){ 
    angle = 4.71239 - atan(fraction);
  }
  else if (trueY <= 0 && trueX <= 0){
    angle = 1.57080 + atan(fraction);
  }
  else if (trueY >= 0 && trueX <= 0){
    angle = 1.57080 - atan(fraction);
  }   
  return(angle);
}

long speedSet(int x, int y){
  long trueX = map(x, 270, 480, -1000, 1000);
  long trueY = map(y, 270, 480, -1000, 1000);
  long modulus= (sqrt(sq(trueX) + sq(trueY)));
  long modulusMap = map(modulus, 0, 1415, 0, 255);
  return ( modulusMap);

}

float currentAngleSet(int x, int y, float z, float t){
  int acceleration = map(x, 0, 1000, -4934, 9707);
  int rudder = map(y, 270, 480, -1000, 1000);
  int value = (z*1000 + (sqrt(acceleration/(100*radius))*t));
  int currentAngle = (value % 6283);  
  return ((currentAngle/1000.0) + fraction*rudder);
}

void controlMotor(int throttle, double heading, double currentAngle, int translationalSpeed, int motor){
  int offset;
  if (motor== 3){ 
    offset = 0;
  }
  if (motor == 5){
    offset = 2094;
  }
  if (motor == 6){
    offset = 4188;
  }
  
  int currentAngleTimes1000 = currentAngle*1000;
  if (translationalSpeed < 50 && throttle > 20){
    analogWrite (motor, throttle);
  }
  
  else if ((((currentAngleTimes1000 + offset - 1570 + 6283) % 6281) < heading*1000) && ((currentAngleTimes1000 + offset + 1570 + 6283) % 6283) > heading*1000) {
    analogWrite (motor, 0);
  }
  else {
    analogWrite (motor, throttle);
  }
}

