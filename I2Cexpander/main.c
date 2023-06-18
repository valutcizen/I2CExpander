// avrdude -p m16 -c usbasp -U flash:w:I2Cexpander.hex:i

#define F_CPU 2000000UL
// Have to be > 16*F_scl, for 100 kHz it should be greather than 1.6 MHz

#include <xc.h>
#include <util/delay.h>
#include <util/twi.h>
#include <avr/interrupt.h>

#define P_SCL PC0
#define P_SDA PC1


uint8_t output_no = 0;
uint8_t input_no = 0;
volatile uint8_t inputs[6];
uint8_t input_keyb_no = 0;
uint8_t input_count[6][8];

void init_twi_slave(uint8_t address)
{
	DDRC &= ~((1 << P_SCL) | (1 << P_SDA));		// Set input SDA, SCL
	PORTC &= ~((1 << P_SCL) | (1 << P_SDA));	// Set high impedance SDA, SCL
	TWAR = (address << 1);						// Set TWI address
	TWCR = (1 << TWEN)							// TWI Enable
		 | (1 << TWIE)							// TWI Interrupt Enable
		 | (1 << TWEA);							// TWI Acknowledge Enable
}

void init_outputs()
{
	PORTB = 0x00;
	PORTD = 0x00;
	DDRB = 0xFF;
	DDRD = 0xFF;
}

void outputs_handler_reset()
{
	output_no = 0;
}

void outputs_handler()
{
	if (output_no == 0)
	{
		PORTB = TWDR;
		output_no++;
	}
	else if (output_no == 1)
	{
		PORTD = TWDR;
		output_no = 0;
	}
}

void inputs_handler_reset()
{
	input_no = 1;
	TWDR = inputs[0];
}

void inputs_handler()
{
	if (input_no >= 6)
	{
		TWDR = 0xFF;
		return;
	}
	
	TWDR = inputs[input_no];
	input_no++;
}

ISR(TWI_vect)
{
	switch (TW_STATUS)
	{
		case TW_SR_SLA_ACK:
			outputs_handler_reset();
		break;
		case TW_SR_DATA_ACK:
			outputs_handler();
		break;
		
		case TW_ST_SLA_ACK:
			inputs_handler_reset();
		break;
		case TW_ST_DATA_ACK:
			inputs_handler();
		default:
		break;
	}

	TWCR |= (1 << TWINT);		// Clear TWI Interrupt flag
}

void init_keyboard()
{
	DDRA = 0x00;									//Port A Input
	PORTA = 0xFF;									//PORT A Pull-up
	PORTC &= (1 << P_SCL) | (1 << P_SDA);			//PORT C All to LOW without SDA, SCL
	DDRC |= (0xFF ^ ((1 << P_SCL) | (1 << P_SDA)));	//PORT C Output without SDA, SCL
	
	for (uint8_t i = 0; i < 6; i++)
	{
		inputs[i] = 0;
		for (uint8_t j = 0; j < 8; j++)
			input_count[i][j] = 0;
	}
}

void handle_keyboard()
{
	input_keyb_no++;
	if (input_keyb_no >= 6)
		input_keyb_no = 0;
	
	uint8_t port_mask = 1 << input_keyb_no;
	PORTC &= ((1 << P_SCL) | (1 << P_SDA));
	if (port_mask >= P_SCL || port_mask >= P_SDA)
		port_mask <<= 1;
	if (port_mask >= P_SCL && port_mask >= P_SDA)
		port_mask <<= 1;
	PORTC |= port_mask;
	_delay_loop_1(40); // = 20 us with F_OSC = 2 MHz
	
	uint8_t new_inputs = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		if (PINA & (1 << i))
		{
			if (input_count[input_keyb_no][i] < 40)
				input_count[input_keyb_no][i]++;
			else
				new_inputs |= 1 << i;
		}
		else
		{
			input_count[input_keyb_no][i] = 0;
		}
	}
	inputs[input_keyb_no] = new_inputs;

}

int main(void)
{
	init_keyboard();		// Initialize keyboard
	init_outputs();			// Initialize outputs
	init_twi_slave(0x20);	// Initialize TWI
	sei();					// Enable interrupts
	
    while(1)
    {
		handle_keyboard();	// Read keyboard, set next to read
    }
}