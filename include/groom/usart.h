#ifndef GROOM_USART_H__
#define GROOM_USART_H__

void usart_init();
void usart_out(char ch);
char usart_in(void);
uint8_t usart_in_timeout(char *c, uint16_t timeout);
void usart_outstring(char *s);
int usart_printf(char *format_string, ...) __attribute__ ((format (printf, 1, 2)));

#endif /* GROOM_USART_H__ */
