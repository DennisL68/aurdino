//Octode beeper engine port from ZX Spectrum to Arduino
//by Shiru (shiru@mail.ru) 11.02.17


//include music data generated by the convert_arduino_octode.py script

#include "music.h"


//general settings

#define SAMPLE_RATE           (3500000/275)     //original Octode runs at 12727 Hz, this code allows to set ~7800 to 96000 Hz (but converter script should be edited first)

#define SPEAKER_PORT          PORTD             //speaker is on the port D
#define SPEAKER_DDR           DDRD
#define SPEAKER_BIT           (1<<7)            //PD7 (Uno pin 7)



//array of counters and reload values, for each channel

unsigned char cnt_value[8];
unsigned char cnt_load [8];

//previous speaker output state

unsigned char output_state;

//'drum' sound synth variables

unsigned char click_drum;
unsigned char click_drum_len;
unsigned char click_drum_cnt_1;
unsigned char click_drum_cnt_2;
unsigned char click_drum_cnt_3;

//sync counter to syncronize the music parser with the synth code
//it is volatile because it the main thread reads it back

volatile unsigned int parser_sync;

//song parser variables

unsigned int song_speed;
unsigned char order_pos;
const unsigned char *pattern;



//load next pattern from the PROGMEM

void new_pattern(void)
{
  while(1)
  {
    //read next pattern pointer from the PROGMEM
    
    pattern=(const unsigned char*)pgm_read_word(&(music_order_list[order_pos++]));

    //if iti s NULL, order list gets looped
    
    if(!pattern) order_pos=music_loop_position; else break;
  }

  //read speed value for new pattern
 
  song_speed =pgm_read_byte_near(pattern);    ++pattern;
  song_speed|=pgm_read_byte_near(pattern)<<8; ++pattern;
}



//initialize all

void setup()
{
	unsigned char i;

	cli();

	//initialize song parser and drum synth variables

	output_state=0;
	parser_sync=0;
	order_pos=0;
  
  click_drum_len=0;
  click_drum_cnt_1=0;
  click_drum_cnt_2=0;
  click_drum_cnt_3=3;

  //initialize channel counters

  for(i=0;i<8;++i)
  {
    cnt_value[i]=0;
    cnt_load[i]=0;
  }
  
  //load first pattern of the song
 
  new_pattern();
   
  //set a port pin as the output
  
	SPEAKER_DDR|=SPEAKER_BIT; 

  //set timer2 to generate interrupts at the sample rate
  
	TCCR2A=0;   
	TCCR2B=0;
	TCNT2 =0;

	TCCR2A|=(1<<WGM21);
	TCCR2B|=(0<<CS02)|(1<<CS01)|(0<<CS00);  //prescaler=8
	TIMSK2|=(1<<OCIE2A);

	OCR2A=16000000/8/SAMPLE_RATE;

	sei();
}



ISR(TIMER2_COMPA_vect)
{
  //output the output state generated in the previous interrupt call as early as possible
  //that's to avoid jitter, because outputting the value near end of the interrupt handler
  //would be happening at slightly different times depending from the counters state
  
  SPEAKER_PORT=output_state;

  //also decrement sync variable early, although this isn't that important, no noticeable jitter possible
  
  if(parser_sync) --parser_sync;

  //clear output state
  
  output_state=0;

  if(!click_drum_len)
  {
     //if the synth in the tone mode, process all counters and set the output bit when any of them gets reloaded
     
    if(cnt_load[0]) { --cnt_value[0]; if(cnt_value[0]==255) { output_state=SPEAKER_BIT; cnt_value[0]=cnt_load[0]; } }
    if(cnt_load[1]) { --cnt_value[1]; if(cnt_value[1]==255) { output_state=SPEAKER_BIT; cnt_value[1]=cnt_load[1]; } }
    if(cnt_load[2]) { --cnt_value[2]; if(cnt_value[2]==255) { output_state=SPEAKER_BIT; cnt_value[2]=cnt_load[2]; } }
    if(cnt_load[3]) { --cnt_value[3]; if(cnt_value[3]==255) { output_state=SPEAKER_BIT; cnt_value[3]=cnt_load[3]; } }
    if(cnt_load[4]) { --cnt_value[4]; if(cnt_value[4]==255) { output_state=SPEAKER_BIT; cnt_value[4]=cnt_load[4]; } }
    if(cnt_load[5]) { --cnt_value[5]; if(cnt_value[5]==255) { output_state=SPEAKER_BIT; cnt_value[5]=cnt_load[5]; } }
    if(cnt_load[6]) { --cnt_value[6]; if(cnt_value[6]==255) { output_state=SPEAKER_BIT; cnt_value[6]=cnt_load[6]; } }
    if(cnt_load[7]) { --cnt_value[7]; if(cnt_value[7]==255) { output_state=SPEAKER_BIT; cnt_value[7]=cnt_load[7]; } }
  }
  else
  {
    //if the synth in the drum mode, generate a beep with a bit of pseudo-random noise
    
    if(((click_drum_cnt_1^click_drum_cnt_2^click_drum_cnt_3)>>click_drum)&1) output_state=SPEAKER_BIT;

    click_drum_cnt_1+=3;
   
    if(click_drum_len&1) click_drum_cnt_2+=18;
  
    if(!(click_drum_len&3)) click_drum_cnt_3+=1;

    --click_drum_len;
  }
}



//main loop

void loop()
{
	unsigned char n;

	while(1)
	{
		//update row
    //read the first byte of a pattern row
    
		n=pgm_read_byte_near(pattern);

    if(n==0xff) //check if it is end of the pattern, load a new one
    {
      new_pattern();

      n=pgm_read_byte_near(pattern);  //re-read first byte of the pattern
    }

    //re-reading pattern causes speed value to be reloaded too, so this is the earliest point to set up the sync counter
    
    parser_sync=song_speed;

    //check if the byte is actually a drum code rather than a note counter value, setup the drum mode if so
    
		if(n>=0xf0)
		{
			click_drum=7-(n-0xf0);
      click_drum_len=128;

			++pattern;  //skip to the next byte
		}

    //read 8 bytes of the pattern and put them to the channel counter reload variables
    
		cnt_load[0]=pgm_read_byte_near(pattern); ++pattern;
		cnt_load[1]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[2]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[3]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[4]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[5]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[6]=pgm_read_byte_near(pattern); ++pattern;
    cnt_load[7]=pgm_read_byte_near(pattern); ++pattern;

		//wait for the next row
    //delay 1 is important, by some reason sng tempo starts to jump a lot without it
 
		while(parser_sync>0) delay(1);
	}

}
