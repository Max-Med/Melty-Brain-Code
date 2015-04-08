#include "avr/interrupt.h"

unsigned int rc[6];
float timer_period;
unsigned int T, T_reset, T_prev, T_offset, T_min, T_max;
unsigned short T_pointer=0;
unsigned int tics_before_reset;
 
void setup()
{ 
  Serial.begin(115200);
  
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
  
  
  double heading = headingSet(rc[1], rc[3]);  //should return angle in radians of vectore (rc[1],rc[3])
  int translationalSpeed = speedSet(rc[1], rc[3]);  //should return magnitude of vector (rc[1],rc[3])
  int throttle= map(rc[6], 270, 480, 0, 255);  //should map rc[6] between 0 and 255
  /*controlMotor(throttle,heading, currentAngle, translationalSpeed, Motor1);
  controlMotor(throttle,heading, currentAngle, translationalSpeed, Motor2);  
  controlMotor(throttle,heading, currentAngle, translationalSpeed, Motor3); */ 
  Serial.print(rc[0]);
  Serial.print("\t");
  Serial.print(rc[1]);
  Serial.print("\t");  
  Serial.print(rc[2]);
  Serial.print("\t");
  Serial.print(rc[3]);
  Serial.print("\t");
  Serial.print(rc[4]);
  Serial.print("\t");
  Serial.print(rc[5]);
  Serial.print("\t"); 
  
  Serial.print(heading*57.3);
  Serial.print("\t");
  Serial.print(translationalSpeed);
  Serial.print("\t");  
  Serial.println(throttle); 
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
 
double headingSet(int x, int y){ 
  int trueX = map(x, 270, 480, -1000, 1000);
  int trueY = map(y, 270, 480, -1000, 1000);
  double fraction = abs (trueY/trueX);
  if (trueY >= 0 && trueX >= 0) {
    return (4.71239 + atan(fraction));
  }
  else if (trueY < 0 && trueX > 0){ 
    return (4.71239 - atan(fraction));
  }
  else if (trueY < 0 && trueX < 0){
    return ( 1.57080 + atan(fraction));
  }
  else if (trueY > 0 && trueX < 0){
    return ( 1.57080 - atan(fraction));
  }   
}

int speedSet(int x, int y){
  int trueX = map(x, 270, 480, -1000, 1000);
  int trueY = map(y, 270, 480, -1000, 1000);
  int modulus= (sqrt(sq(trueX)^2 + sq(trueY)));
  return ( map(modulus, 0, 1415, 0, 255));

}

//void controlMotor(int throttle, double heading, double currentAngle, int translationalSpeed, int Motor){
  

