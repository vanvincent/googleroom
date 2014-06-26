#ifndef GROOM_ENCODER_H_
#define GROOM_ENCODER_H_

#define ENCODER_PORT  PIND
#define ENCODER_DDR   DDRD
#define ENCODER_A_PIN 5
#define ENCODER_B_PIN 6

void encoder_init(void);

uint8_t encoder_sample(void);

int16_t encoder_val(void);

#endif /* GROOM_ENCODER_H_ */