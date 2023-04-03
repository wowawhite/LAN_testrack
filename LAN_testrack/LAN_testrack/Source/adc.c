/*
 * adc.c
 * 
 * Continuously sampling ADC driver.
 * 
 * Author:      Sebastian Goessl
 * Hardware:    ATmega328P
 * 
 * LICENSE:
 * MIT License
 * 
 * Copyright (c) 2018 Sebastian Goessl
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */



#include <avr/io.h>         //hardware registers
#include <avr/interrupt.h>  //interrupt vectors
#include <util/atomic.h>    //atomic blocks
#include "adc.h"



//default to Arduino oscillator
#ifndef F_CPU
    #define F_CPU 16000000UL
    #warning "F_CPU not defined! Assuming 16MHz."
#endif


//find fitting prescaler
#if F_CPU/2 <= ADC_FREQUENCY_MAX && F_CPU/2 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 2
    #define ADPS0_VALUE 0
    #define ADPS1_VALUE 0
    #define ADPS2_VALUE 0
#elif F_CPU/4 <= ADC_FREQUENCY_MAX && F_CPU/4 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 4
    #define ADPS0_VALUE 0
    #define ADPS1_VALUE 1
    #define ADPS2_VALUE 0
#elif F_CPU/8 <= ADC_FREQUENCY_MAX && F_CPU/8 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 8
    #define ADPS0_VALUE 1
    #define ADPS1_VALUE 1
    #define ADPS2_VALUE 0
#elif F_CPU/16 <= ADC_FREQUENCY_MAX && F_CPU/16 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 16
    #define ADPS0_VALUE 0
    #define ADPS1_VALUE 0
    #define ADPS2_VALUE 1
#elif F_CPU/32 <= ADC_FREQUENCY_MAX && F_CPU/32 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 32
    #define ADPS0_VALUE 1
    #define ADPS1_VALUE 0
    #define ADPS2_VALUE 1
#elif F_CPU/64 <= ADC_FREQUENCY_MAX && F_CPU/64 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 64
    #define ADPS0_VALUE 0
    #define ADPS1_VALUE 1
    #define ADPS2_VALUE 1
#elif F_CPU/128 <= ADC_FREQUENCY_MAX && F_CPU/128 >= ADC_FREQUENCY_MIN
    #define ADC_PRESCALER 128
    #define ADPS0_VALUE 1
    #define ADPS1_VALUE 1
    #define ADPS2_VALUE 1
#else
    #error "No valid ADC_PRESCALER found!"
#endif



//channel indices of the currently sampling channel and the next one
static volatile size_t adc_current = 0, adc_next = 0;
//last samples
static volatile uint16_t adc_channels[ADC_N] = {0};



void adc_init(void)
{
    ADMUX |= (1 << REFS0);
    ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (1 << ADIE)
        | (ADPS2_VALUE << ADPS2) | (ADPS1_VALUE << ADPS1)
        | (ADPS0_VALUE << ADPS0);
}



uint16_t adc_get(size_t channel)
{
    uint16_t value;
    
    //use atomic block because the values are 16 bit and
    //therefore can't be read in a single clock cycle
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = adc_channels[channel];
    }
    
    return value;
}

double adc_getScaled(size_t channel)
{
    return (double)adc_get(channel) / ADC_TOP;
}

void adc_getAll(uint16_t *channels)
{
    size_t i;
    
    for(i=0; i<ADC_N; i++)
        *channels++ = adc_get(i);
}

void adc_getAllScaled(double *channels)
{
    size_t i;
    
    for(i=0; i<ADC_N; i++)
        *channels++ = adc_getScaled(i);
}



ISR(ADC_vect)
{
    //save sampled value
    adc_channels[adc_current] = ADC;
    
    //advance indices
    adc_current = adc_next;
    if(++adc_next >= ADC_N)
        adc_next = 0;
    //adc_next = (adc_next + 1) % ADC_N;
    
    //ADC is already sampling channel adc_current
    //and will sample adc_next in the next cycle
    ADMUX = (ADMUX & ((1 << REFS0) | (1 << REFS0) | (1 << ADLAR))) |
        (adc_next & ((1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (1 << MUX0)));
}
