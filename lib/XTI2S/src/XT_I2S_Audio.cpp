// XTronical I2S Audio Library, currently supporting ESP32
// May work with other processors and/or I2S DAC's with or without modifications
// (c) XTronical 2018-2021, Licensed under GNU GPL 3.0 and later, under this license absolutely no warranty given
// See www.xtronical.com for documentation
// This software is currently under active development (Feb 2021) and may change and break your code with new versions
// No responsibility is taken for this. Stick with the version that works for you, if you need newer commands from later versions
// you may have to alter your original code

#include <XT_I2S_Audio.h>
#include <math.h>    
#include <cstring>
#include <HardwareSerial.h>

WavHeader_Struct WavHeader;						// Used as a place to store the header data from the wav data
XT_Instrument_Class DEFAULT_INSTRUMENT; 		// Used if calling routine does not state an instrument for a note.

// The FNOTE "defines" below contain actual frequencies for notes which range from a few Hz (around 30) to around 4000Hz
// But in essence there are only 89 different notes , we collect them into an array so that we can store a note as
// a number from 0 to 90, (0 being no 0Hz, or silence). In this way we can store musical notes in single bytes.
// To benefit from this the music data in your code should be greater than 90 bytes otherwise your technically using more
// memory than you would if you used the raw frequencies (which use 2 bytes per note). It is estimated fin simple projects you
// would not gain but generally a simple project would have more spare memory anyway. For larger projects where memory could
// be more of an issue you will actually save memory, so hopefully a win!
uint16_t XT_Notes[90]={1,FNOTE_B0,FNOTE_C1,FNOTE_CS1,FNOTE_D1,FNOTE_DS1,FNOTE_E1,FNOTE_F1,FNOTE_FS1,FNOTE_G1,FNOTE_GS1,FNOTE_A1,FNOTE_AS1,FNOTE_B1,FNOTE_C2,FNOTE_CS2,FNOTE_D2,FNOTE_DS2,FNOTE_E2,FNOTE_F2,FNOTE_FS2,FNOTE_G2,FNOTE_GS2,FNOTE_A2,FNOTE_AS2,FNOTE_B2,FNOTE_C3,FNOTE_CS3,FNOTE_D3,FNOTE_DS3,FNOTE_E3,FNOTE_F3,FNOTE_FS3,FNOTE_G3,FNOTE_GS3,FNOTE_A3,FNOTE_AS3,FNOTE_B3,FNOTE_C4,FNOTE_CS4,FNOTE_D4,FNOTE_DS4,FNOTE_E4,FNOTE_F4,FNOTE_FS4,FNOTE_G4,FNOTE_GS4,FNOTE_A4,FNOTE_AS4,FNOTE_B4,FNOTE_C5,FNOTE_CS5,FNOTE_D5,FNOTE_DS5,FNOTE_E5,FNOTE_F5,FNOTE_FS5,FNOTE_G5,FNOTE_GS5,FNOTE_A5,FNOTE_AS5,FNOTE_B5,FNOTE_C6,FNOTE_CS6,FNOTE_D6,FNOTE_DS6,FNOTE_E6,FNOTE_F6,FNOTE_FS6,FNOTE_G6,FNOTE_GS6,FNOTE_A6,FNOTE_AS6,FNOTE_B6,FNOTE_C7,FNOTE_CS7,FNOTE_D7,FNOTE_DS7,FNOTE_E7,FNOTE_F7,FNOTE_FS7,FNOTE_G7,FNOTE_GS7,FNOTE_A7,FNOTE_AS7,FNOTE_B7,FNOTE_C8,FNOTE_CS8,FNOTE_D8,FNOTE_DS8
};

// Precalculate sine values in a certain range (i.e. usually 0 to 360Deg), we want a new measure of a Degree that
// defines a circle as X number of parts. The greater the number of parts the greater the resolution of the sine wave

int16_t SineValues[SINE_RESOLUTION];       				// an array to store our values for sine, which speeds things up 
														// instead of calc'ing in the fly

void InitSineValues()
{
	float ConversionFactor=(2.0*M_PI)/float(SINE_RESOLUTION);      // convert my SINE bits in a circle to radians
														// there are 2 x PI radians in a circle hence the 2*PI
														// Then divide by SINE_RESOLUTION to get the value in radians
														// for one of my SINE_RESOLUTION bits.
	float RadAngle;                           			// Angle in Radians, have to have this as computers love radians!
	// calculate sine values
	for(int MyAngle=0;MyAngle<SINE_RESOLUTION;MyAngle++) {
		RadAngle=MyAngle*ConversionFactor;              // angle converted to radians
		SineValues[MyAngle]=sin(RadAngle)*32767;  		// get the sine of this angle and shift it into the range of our dac
														// which is signed 16 bit integers, so max positive is 32767
	}
}

/**********************************************************************************************************************/
/* global functions                                                                                                   */
/**********************************************************************************************************************/

void SetVolume(int16_t* Left,int16_t* Right,int16_t Volume)
{
	// returns the sound samples adjusted for the volume passed, volume is passed as a %
	// 0 would cause silence, 50 would be half volume, 100 would be the original volume (unchanged).  Anything above
	// 100 would cause the volume to increase (i.e. gain); A negative value will leave volume alone and return 
	// original values passed in
	
	float Vol=float(Volume)/100;
	if(Volume<0)
		return;	
	*Left=(*Left)*Vol;
	*Right=(*Right)*Vol;
}



/**********************************************************************************************************************/
/* XT_PlayListItem_Class                                                                                              */
/**********************************************************************************************************************/

XT_PlayListItem_Class::XT_PlayListItem_Class()
{
	RepeatForever=false;
	Repeat=0;
	RepeatIdx=0;
}

// Despite being marked virtual and being overridden in desendents without these I get vtable error
// issues which I really don't understand, so I put these in even though never called and all is well
void XT_PlayListItem_Class::NextSample(int16_t* Left, int16_t* Right)
{
	
}
void XT_PlayListItem_Class::Init()
{
}

/**********************************************************************************************************************/
/* Wave class 			                                                                                              */
/**********************************************************************************************************************/
// Despite being marked virtual and being overridden in desendents without these I get vtable error
// issues which I really don't understand, so I put these in even though never called and all is well
void XT_Wave_Class::NextSample(int16_t* Left, int16_t* Right)
{
	
}
void XT_Wave_Class::Init(int8_t Note)
{
}


/**********************************************************************************************************************/
/* Envelope class, see www.xtronical.com for description of how to use envelopes with the instrument class            */
/**********************************************************************************************************************/


XT_Envelope_Class::XT_Envelope_Class()
{
	// nothing yet
}

XT_Envelope_Class::~XT_Envelope_Class()
{
	// delete any further linked envelopes and associated envelope parts for all envelopes found
	// Note if you delete from an envelope that it is not the start envelope then the previous
	// envelopes will still exist and in fact the last one before the one you delete will still
	// point to this one. If for some reason you wish to delete a none first envelope then you
	// must tidy up the last of the previous envelopes yourself.

	XT_EnvelopePart_Class* Part,*NextPart;

	if(NextEnvelope!=0)
		delete(NextEnvelope);				// if another envelope in the chain delete this also

	// delete the parts for this envelope
	Part=FirstPart;
	while(Part!=0)
	{
		NextPart=Part->NextPart;
		delete(Part);
		Part=NextPart;
	}
}


void XT_Envelope_Class::Init()
{
	// Reset envelope ready for next note to play
	CurrentEnvelopePart=0;
	EnvelopeCompleted=false;
	RepeatCounter=Repeats;
	DurationCounter=1;
}

void XT_Envelope_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// are there any parts, if not do nothing
	if(FirstPart!=0)
	{
		// OK we have some envelope parts, manipulate the wave form volume according to the
		// current envelope part in use for this instrument
		if(EnvelopeCompleted==false)
		{
			if(CurrentEnvelopePart==0)  				// start of envelope
			{
				CurrentEnvelopePart=FirstPart;
				InitEnvelopePart(CurrentEnvelopePart,CurrentVolume);
			}
			if(CurrentEnvelopePart->Completed)
			{
				CurrentEnvelopePart=CurrentEnvelopePart->NextPart;
				if(CurrentEnvelopePart==0)  // no more parts, do we need to repeat the envelope
				{
					if(RepeatCounter>0)
						RepeatCounter--;
					else
						EnvelopeCompleted=true;
				}
				else
					InitEnvelopePart(CurrentEnvelopePart,CurrentVolume);
			}
			if(CurrentEnvelopePart!=0)
			{
				CurrentVolume+=VolumeIncrement;
				SetVolume(Left,Right,CurrentVolume);
				DurationCounter--;
				if(DurationCounter==0)
					CurrentEnvelopePart->Completed=true;
			}
		}
	}
}




void XT_Envelope_Class::InitEnvelopePart(XT_EnvelopePart_Class* EPart,uint8_t LastVolume)
{
	// initialises the properties in preparation to starting to use this envelope object
	// in note production

	if(EPart->StartVolume!=-1)					// do we have a start vol
		LastVolume=EPart->StartVolume;			// yes , set to this
	DurationCounter=EPart->RawDuration;
	// calculate how much the volume should increment per sample, this depends on what
	// the last volume reached was for the last envelope part, initially for the first
	// envelope part this would be 0. Volume is in the range 0-127
	VolumeIncrement=float((EPart->TargetVolume-LastVolume))/float(DurationCounter);
	CurrentVolume=LastVolume;
	EPart->Completed=false;
}



XT_EnvelopePart_Class* XT_Envelope_Class::AddPart(uint16_t Duration,int16_t StartVolume,int16_t TargetVolume)
{
	XT_EnvelopePart_Class* EPart;
	EPart=AddPart(Duration,TargetVolume);
	EPart->StartVolume=StartVolume;
	return EPart;
}



XT_EnvelopePart_Class* XT_Envelope_Class::AddPart(uint16_t Duration,int16_t TargetVolume)
{
	// creates and adds an envelope part to this current envelope
	XT_EnvelopePart_Class* EPart=new XT_EnvelopePart_Class();

	EPart->SetDuration(Duration);
	EPart->TargetVolume=TargetVolume;
	if(FirstPart==0)											// First in list of envelope parts
	{
		FirstPart=EPart;
		LastPart=EPart;
	}
	else														// Add to end of list of envelope parts
	{
		LastPart->NextPart=EPart;
		LastPart=EPart;
	}
	return EPart;
}





// Envelope part class
void XT_EnvelopePart_Class::SetDuration(uint16_t Duration)
{
	this->Duration=Duration;
	this->RawDuration=50*Duration;
}

uint16_t XT_EnvelopePart_Class::GetDuration()
{
	return Duration;
}



/************************************************************************************************************************/
/* Wave classes (for basic sounds)                                                                               		*/
/************************************************************************************************************************/

// Square wave

void XT_SquareWave_Class::NextSample(int16_t *Left,int16_t *Right)
{
	// returns the next samples for this frequency of a square wave
	Counter--;
	if(Counter<0)
	{
		// flip the wave output 
		Counter+=CounterStartValue;				// as this can be a decimal, any amount extra below zero
												// is taken away from next starting value, in this way
												// for higher frequencies we do not lose as much
												// accuracy over a few waves, they average out to be
												// about right "on average"
		if(CurrentLeftSample==MAX_WAVE_HEIGHT)  // we are high so flip low
		{
			CurrentLeftSample=MIN_WAVE_HEIGHT;
			CurrentRightSample=MIN_WAVE_HEIGHT;
		}
		else									 // we are LOW so flip HIGH
		{
			CurrentLeftSample=MAX_WAVE_HEIGHT;
			CurrentRightSample=MAX_WAVE_HEIGHT;
		}
	}

	*Left=CurrentLeftSample;
	*Right=CurrentRightSample;
}

void XT_SquareWave_Class::Init(int8_t Note)
{
	if(Note!=-1)								// use the note not a raw frequency
		Frequency=XT_Notes[Note];  				// if -1 then use the raw frequency
	if(Frequency>SAMPLES_PER_SEC/2)
		Frequency=SAMPLES_PER_SEC/2;
	if(Frequency!=0)							// avoid divide by 0, a freq of 0 means no sound
	{
		CounterStartValue=((SAMPLES_PER_SEC/2)/float(Frequency));
		Counter=CounterStartValue;
	}
	else
		Counter=0;
}

// Sine

void XT_SineWave_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// returns the next sample for this frequency of a sine wave
	CurrentAngle+=AngleIncrement;
	if(CurrentAngle>(SINE_RESOLUTION-1))
		CurrentAngle=0;	
	*Left=SineValues[int(CurrentAngle)];		
	*Right=SineValues[int(CurrentAngle)];	
}

void XT_SineWave_Class::Init(int8_t Note)
{
	CurrentAngle=0;
	if(Note!=-1)								// use the note not a raw frequency
		Frequency=XT_Notes[Note];  				// if -1 then use the raw frequency

	if(Frequency>SAMPLES_PER_SEC/2)				// max frequency cannot be more than half of max samples/sec, and even so
		Frequency=SAMPLES_PER_SEC/2;			// at this frequency it would be more like a square wave than sine due
												// to lack of resolution at this freq.
	if(Frequency!=0)							// avoid divide by 0, a freq of 0 means no sound
		AngleIncrement=SINE_RESOLUTION/(SAMPLES_PER_SEC/float(Frequency));   // determines frequency
}


// Sawtooth
void XT_SawToothWave_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// returns the next byte for this frequency of a square wave

	NextAmplitude+=ChangeAmplitudeBy;
	if(NextAmplitude>32767)  // top of a peak, right down to bottom again
		NextAmplitude=-32768;
	*Left=NextAmplitude;
	*Right=NextAmplitude;
}

void XT_SawToothWave_Class::Init(int8_t Note)
{
	NextAmplitude=0;
	if(Note!=-1)								// use the note not a raw frequency
		Frequency=XT_Notes[Note];  				// if -1 then use the raw frequency

	if(Frequency>SAMPLES_PER_SEC/2)
		Frequency=SAMPLES_PER_SEC/2;
	if(Frequency!=0)							// avoid divide by 0, a freq of 0 means no sound
		ChangeAmplitudeBy=65536/(SAMPLES_PER_SEC/float(Frequency)); // determines amplitude change per sample
}

void XT_TriangleWave_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// returns the next byte for this frequency of a triangle wave
	// called at the raw sample rate
	NextAmplitude+=ChangeAmplitudeBy;
	if(NextAmplitude>32767)
	{	// top of a peak, reverse direction
		ChangeAmplitudeBy=-ChangeAmplitudeBy;    	// reverse direction
		NextAmplitude=32767+ChangeAmplitudeBy;
	}
	else
	{
		if(NextAmplitude<-32768)
		{	// bottom of a trough, reverse direction
			ChangeAmplitudeBy=-ChangeAmplitudeBy;    	// reverse direction
			NextAmplitude=32768+ChangeAmplitudeBy;
		}
	}
	*Left=NextAmplitude;
	*Right=NextAmplitude;
}

void XT_TriangleWave_Class::Init(int8_t Note)
{
	NextAmplitude=0;
	if(Note!=-1)								// use the note not a raw frequency
		Frequency=XT_Notes[Note];  				// if -1 then use the raw frequency

	if(Frequency>SAMPLES_PER_SEC/2)
		Frequency=SAMPLES_PER_SEC/2;
	if(Frequency!=0)							// avoid divide by 0, a freq of 0 means no sound
	{
		ChangeAmplitudeBy=32768/((SAMPLES_PER_SEC/2)/float(Frequency));

	}
}


/**********************************************************************************************************************/
/* XT_WAV_Class                                                                                              		*/
/**********************************************************************************************************************/


#define DATA_CHUNK_ID 0x61746164
#define FMT_CHUNK_ID 0x20746d66
// Convert 4 byte little-endian to a long.
#define longword(bfr, ofs) (bfr[ofs+3] << 24 | bfr[ofs+2] << 16 |bfr[ofs+1] << 8 |bfr[ofs+0])

XT_Wav_Class::XT_Wav_Class(const unsigned char *WavData)
{	
    // create a new wav class object
	memcpy(&WavHeader,WavData,44);                     // Copy the header part of the wav data into our structure
	SampleRate=WavHeader.SampleRate;
	BytesPerSample=(WavHeader.BitsPerSample/8)*WavHeader.NumChannels;
	DataSize=WavHeader.DataSize;
    IncreaseBy=float(SampleRate)/SAMPLES_PER_SEC;
    PlayingTime = (1000 * DataSize) / (uint32_t)(SampleRate);
    Data=WavData+44;
    Speed=1.0;
	Volume=100;
}


void XT_Wav_Class::Init()
{
	LastIntCount=0;
	if(Speed>=0)
		DataIdx=DataStart;				// reset data pointer back to start of WAV data
	else
		DataIdx=DataStart+DataSize;		// reset data pointer back to end of WAV data
	Count=0;
	SpeedUpCount=0;
	TimeElapsed = 0;
	TimeLeft = PlayingTime;
}


void XT_Wav_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// Returns the next samples to be played, note that this routine will return values suitable to
	// be played back at SAMPLES_PER_SEC. 
	// If mono then will return same data on left and right channels


	uint32_t IntPartOfCount;
	float ActualIncreaseBy;
	float CopyOfSpeed=Speed;							// Copy into this var as it could potentially change during this routine giving odd results
	// increase the counter, if it goes to a new integer digit then write to DAC

	ActualIncreaseBy=IncreaseBy;      // default if not playing slower than normal
	if(CopyOfSpeed<=1.0)	// manipulate IncreaseBy
		ActualIncreaseBy=IncreaseBy*(abs(CopyOfSpeed));
	Count+=ActualIncreaseBy;
	IntPartOfCount=floor(Count);
	// return previous value by default
	*Left=(Data[DataIdx+1]<<8)+Data[DataIdx];	// Get last left channel Data
	*Right=(Data[DataIdx+3]<<8)+Data[DataIdx+2]; // Get last right channel Data
    SetVolume(Left,Right,Volume); 
	if(IntPartOfCount>LastIntCount)
	{
		if(CopyOfSpeed>=0) // Playing in forward direction
		{
			if(CopyOfSpeed>1.0)
			{
				// for speeding up we need to basically go through the data quicker as upping the frequency
				// that this routine is called is not an option as many sounds can potentially play through it
				// and we have speed control over each. 

				// we now get the integer and decimal parts of this number and move the DataIdx on by "int" amount first
				double IntPartAsFloat,DecimalPart,TempSpeed;
				TempSpeed=CopyOfSpeed-1.0;                              
				DecimalPart=modf(TempSpeed,&IntPartAsFloat);
				DataIdx+=BytesPerSample*int(IntPartAsFloat);			// always increase by the integer part
				SpeedUpCount+=DecimalPart;								//This keeps track of the decimal part
				// If SpeedUpCount >1 then add this extra sample to the DataIdx too and subtract 1 from SpeedUpCount
				// This allows us "apparently" increment the DataIdx by a decimal amount
				if(SpeedUpCount>=1)
				{
					DataIdx+=BytesPerSample;				// move another pos into data
					SpeedUpCount--;			// Take it off SpeedUpCount
				}
			}
			// gone to a new integer of count, we need to send a new value to the I2S DAC next time
			// update the DataIDx counter

			LastIntCount=IntPartOfCount;
			DataIdx+=4;							// 4, because 2 x 16bit samples
			TimeElapsed = 1000 * DataIdx / BytesPerSample;
			TimeLeft = PlayingTime - TimeElapsed;
			if(DataIdx>=DataSize)  				// end of data, flag end
			{
				Count=0;						// reset frequency counter
				DataIdx=DataStart;				// reset data pointer back to beginning of WAV data
				Playing=false;  				// mark as completed
				TimeLeft = 0;
			}
		}
		else
		{
			// Playing sound in reverse 
			if(CopyOfSpeed<-1.0)
			{
				// for speeding up we need to basically go backwards through the data quicker as upping the frequency
				// that this routine is called is not an option as many sounds can potentially play through it
				// and we have speed control over each. 

				// we now get the integer and decimal parts of this number and move the DataIdx on by "int" amount first
				double IntPartAsFloat,DecimalPart,TempSpeed;
				TempSpeed=CopyOfSpeed+1.0;                              
				DecimalPart=abs(modf(TempSpeed,&IntPartAsFloat));
				DataIdx-=BytesPerSample*int(IntPartAsFloat);			// always decrease by the integer part
				SpeedUpCount+=DecimalPart;								//This keeps track of the decimal part
				// If SpeedUpCount >1 then add this extra sample to the DataIdx too and subtract 1 from SpeedUpCount
				// This allows us "apparently" increment the DataIdx by a decimal amount
				if(SpeedUpCount>=1)
				{
					DataIdx-=BytesPerSample;				// move another pos into data
					SpeedUpCount--;			// Take it off SpeedUpCount
				}
			}
			// gone to a new integer of count, we need to send a new value to the I2S DAC next time
			// update the DataIDx counterz

			LastIntCount=IntPartOfCount;   
			DataIdx-=4;							// 4, because 2 x 16bit samples
			TimeElapsed = PlayingTime-(1000 * DataIdx / BytesPerSample);
			TimeLeft = PlayingTime - TimeElapsed;
			if(DataIdx<0)  				// end of data, flag end
			{

				Count=0;						// reset frequency counter
				DataIdx=DataStart+DataSize;		// reset data pointer back to end of WAV data
				Playing=false;  				// mark as completed
				TimeLeft = 0;
			}
		}
	}
}



/************************************************************************************************************************/
/* Instrument Class                                                                                            			*/
/************************************************************************************************************************/

XT_Instrument_Class::XT_Instrument_Class():XT_Instrument_Class(INSTRUMENT_PIANO,-1)
{
	  // See main constructor routine for description of passed parameters
	  // and defaults
}


XT_Instrument_Class::XT_Instrument_Class(int16_t InstrumentID):XT_Instrument_Class(InstrumentID,-1)
{
	  // See main constructor routine for description of passed parameters
	  // and defaults
}


XT_Instrument_Class::XT_Instrument_Class(int16_t InstrumentID, int16_t Volume)
{
	this->Note=abs(NOTE_C4);					// default note
	this->SoundDuration=1000;					// default note length, ignored if using envelopes
	this->Duration=1000;						// default length of entire play action (i.e. after any decay) ignored if using envelopes
	if(Volume<0)
		Volume=100;
	this->Volume=Volume;
	SetInstrument(InstrumentID);				// The default

}


void XT_Instrument_Class::SetNote(int8_t Note)
{
	this->Note=abs(Note);
}


void XT_Instrument_Class::SetInstrument(uint16_t Instrument)
{
	// To create your new "permanent" instrument to the code, add a line here that calls its
	// set up routine and add this as a private function to the header file for the
	// Instrument class. Also add a suitable # define to the Instruments.h header file
	// in the instruments section.
	// Any envelopes and waveforms will have their memory cleared here, no need to worry!

	delete(FirstEnvelope);				// remove old envelope definition
	FirstEnvelope=nullptr;              // TEB, Oct-10-2019

	switch (Instrument)
	{
		case(INSTRUMENT_NONE) 			: SetDefaultInstrument();break;
		case(INSTRUMENT_PIANO)			: SetPianoInstrument();break;
		case(INSTRUMENT_HARPSICHORD)	: SetHarpsichordInstrument();break;
		case(INSTRUMENT_ORGAN)			: SetOrganInstrument();break;
		case(INSTRUMENT_SAXOPHONE)		: SetSaxophoneInstrument();break;

		default: // compilation error, default to just square wave
			SetDefaultInstrument();break;
	}
}


void XT_Instrument_Class::SetDefaultInstrument()
{
	SetWaveForm(WAVE_SQUARE);
}


void XT_Instrument_Class::SetPianoInstrument()
{
	SetWaveForm(WAVE_SINE);
	FirstEnvelope=new XT_Envelope_Class();
	FirstEnvelope->AddPart(25,1);
	FirstEnvelope->AddPart(20,0.16);
	FirstEnvelope->AddPart(15,0.63);
	FirstEnvelope->AddPart(10,0.12);
	FirstEnvelope->AddPart(5,0.39);
	FirstEnvelope->AddPart(300,0);
}

void XT_Instrument_Class::SetHarpsichordInstrument()
{
	SetWaveForm(WAVE_SAWTOOTH);
	FirstEnvelope=new XT_Envelope_Class();
	FirstEnvelope->AddPart(15,127);
	FirstEnvelope->AddPart(80,100);
	FirstEnvelope->AddPart(300,0);
}


void XT_Instrument_Class::SetOrganInstrument()
{
	SetWaveForm(WAVE_SINE);
	FirstEnvelope=new XT_Envelope_Class();
	FirstEnvelope->AddPart(15,127);
	FirstEnvelope->AddPart(3000,0);	// An organ maintains until key released
}



void XT_Instrument_Class::SetSaxophoneInstrument()
{
	SetWaveForm(WAVE_SQUARE);
	FirstEnvelope=new XT_Envelope_Class();
	FirstEnvelope->AddPart(15,127);
	FirstEnvelope->AddPart(3000,0);	// An organ maintains until key released
}


void XT_Instrument_Class::SetWaveForm(uint8_t WaveFormType)
{
	// Sets the wave form for this instrument

	delete (WaveForm);										// free any previous memory for a previous waveform
	switch (WaveFormType)
	{
		case WAVE_SQUARE : WaveForm=new XT_SquareWave_Class();break;
		case WAVE_TRIANGLE : WaveForm=new XT_TriangleWave_Class();break;
		case WAVE_SAWTOOTH : WaveForm=new XT_SawToothWave_Class();break;
		case WAVE_SINE : WaveForm=new XT_SineWave_Class();break;

		default: // compilation error, default to square
			WaveForm=new XT_SquareWave_Class();break;
	}
}

void XT_Instrument_Class::SetFrequency(uint16_t Freq)
{
	// only if not using note property
	Note=-1;
	WaveForm->Frequency=Freq;
	WaveForm->Init(-1);
}


void XT_Instrument_Class::Init()
{
	CurrentByte=0;
	WaveForm->Init(Note);
	// Note : These next two lines calculating the counters for the note duration are not used
	// if there are envelopes attached to this instrument, the timings of the envelopes taken
	// priority
	if(FirstEnvelope!=nullptr)           
	{
		CurrentEnvelope=FirstEnvelope;
		CurrentEnvelope->Init();

	}
}

void XT_Instrument_Class::SetDuration(uint32_t Duration)
{
	this->Duration=Duration;
	DurationCounter=this->Duration*(float(SAMPLES_PER_SEC)/1000);
}

void XT_Instrument_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// If no envelope then use normal duration settings
	//Serial.println(DurationCounter);
	if(CurrentEnvelope==nullptr)         
	{
		if(DurationCounter==0)								// If sound completed return no sound
		{
			*Left=0;
			*Right=0;
			return;
		}
		DurationCounter--;
	}

	uint8_t ByteToPlay;
	if(WaveForm->Frequency==0)										// If no frequency then no sound
	{
		*Left=0;
		*Right=0;
		return;
	}
	//if(DumpData)
	//	Serial.println(*Left);

	// This next bit handles basic wave form if a sound is being produced

	WaveForm->NextSample(Left,Right);

	// Next add in envelope control if there is one
	if(CurrentEnvelope!=nullptr)                              
	{
		CurrentEnvelope->NextSample(Left,Right);
		if(CurrentEnvelope->EnvelopeCompleted)
		{
			// check if another envelope in chain
			if(CurrentEnvelope->NextEnvelope!=nullptr)        
			{
				CurrentEnvelope=CurrentEnvelope->NextEnvelope;
				CurrentEnvelope->Init();
			}
			else
			{
				Playing=false;		// All envelopes played
			}
		}
	}
	// adjust for current volume of this sound, the line below was causing issues, put back in when investigated/sorted
	//SetVolume(Left,Right,Volume);
}



//Music score class

XT_MusicScore_Class::XT_MusicScore_Class(int8_t* Score):XT_MusicScore_Class(Score,TEMPO_ALLEGRO,&DEFAULT_INSTRUMENT)
{
	// set tempo and instrument to default, number of plays to 1 (once)
}


XT_MusicScore_Class::XT_MusicScore_Class(int8_t* Score,uint16_t Tempo):XT_MusicScore_Class(Score,Tempo,&DEFAULT_INSTRUMENT)
{
	// set instrument to default and plays to 1
}


XT_MusicScore_Class::XT_MusicScore_Class(int8_t* Score,uint16_t Tempo,XT_Instrument_Class* Instrument)
{
	// Create music score
	this->Score=Score;
	this->Tempo=Tempo;
	this->Instrument=Instrument;
}


XT_MusicScore_Class::XT_MusicScore_Class(int8_t* Score,uint16_t Tempo,uint16_t InstrumentID)
{
	// Create music score
	this->Score=Score;
	this->Tempo=Tempo;
	this->Instrument=&DEFAULT_INSTRUMENT;
	Instrument->SetInstrument(InstrumentID);
}


void XT_MusicScore_Class::Init()
{
	// Called before as score starts playing

	Instrument->Init(); // Initialise instrument

	// convert the tempo in BPM into a value that counts down in sample rate per second 
	ChangeNoteEvery=(60/float(Tempo))*SAMPLES_PER_SEC;
	ChangeNoteCounter=0;					// ensures starts by playing first note with no delay
	ScoreIdx=0;								// set position to first no
}


void XT_MusicScore_Class::NextSample(int16_t* Left,int16_t* Right)
{
	// returns next byte for the DAC

	// are we ready to play another note?
	if(ChangeNoteCounter==0)  {
		// Yes, new note
		if(Score[ScoreIdx]==SCORE_END)  // end of data
		{
			Playing=false;
			// return silence;
			Left=0;
			Right=0;    	
			return;
		}

		Instrument->Note=abs(Score[ScoreIdx]);			// convert the negative value to positive index.
		ScoreIdx++;										// move to next note

		// set length of play for instrument
		// Check next data value to see if it is a beat value
		if(Score[ScoreIdx]>0) 							// positive value, therefore not default beat length
		{
			// Set the duration, beat value of 1=0.25 beat, 2 0.5 beat etc. So just divide by 4
			// to get the real beat length,
			Instrument->Duration=(ChangeNoteEvery*(float(Score[ScoreIdx])/4));
			// Then times by 0.8 to allow for natural movement
			// of players finger on the instrument.
			Instrument->SoundDuration=Instrument->Duration*0.8;
			ChangeNoteCounter=Instrument->Duration;      	// set back to start of count ready for next note
			ScoreIdx++;										// point to next note, ready for next time
		}
		else
		{
			// default single beat values
			Instrument->Duration=ChangeNoteEvery;
			Instrument->SoundDuration=Instrument->Duration*0.8;  	// By default a note plays for 80% of tempo
																// this allows for the natural movement of the
																// player performing the next note
			ChangeNoteCounter=ChangeNoteEvery;              	// set back to start of count ready for next note
		}
		// Note that we do not call DacAudio.Play() here as it's already been done for this music score
		// and it's this music score that effectively controls the note playing, however we still need
		// to initialise the instrument for each new note played
		Instrument->Init();
	}
	ChangeNoteCounter--;
	// return the next byte for this instrument
	Instrument->NextSample(Left,Right);
	
}




void XT_MusicScore_Class::SetInstrument(uint16_t InstrumentID)
{
	Instrument->SetInstrument(InstrumentID);
}





/**********************************************************************************************************************/
/* XT_I2S_Class                                                                                                       */
/**********************************************************************************************************************/

XT_I2S_Class::XT_I2S_Class(uint8_t LRCLKPin,uint8_t BCLKPin,uint8_t I2SOutPin,i2s_port_t PassedPortNum)
{
	// Set up I2S config structure8
    // Port number is either 0 or 1, I2S_NUM_0, I2S_NUM_1, at least for ESP32 it is
	
	 i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
     i2s_config.sample_rate = SAMPLES_PER_SEC;                       // Note, max sample rate
     i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
     i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
     i2s_config.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB);
     i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;             // high interrupt priority
     i2s_config.dma_buf_count = 2;                                   // 8 buffers
     i2s_config.dma_buf_len = 256;                                   // 256 bytes per buffer, so 2K of buffer space
     i2s_config.use_apll=0;
     i2s_config.tx_desc_auto_clear= true; 
     i2s_config.fixed_mclk=-1;    


	// Set up I2S pin config structure
 	pin_config.bck_io_num = BCLKPin;                        // The bit clock connection
    pin_config.ws_io_num = LRCLKPin;                        // Word select, also known as left right clock
    pin_config.data_out_num = I2SOutPin;                    // Data out from the ESP32, connect to DIN on 38357A
    pin_config.data_in_num = I2S_PIN_NO_CHANGE;             // we are not interested in I2S data into the ESP32

	PortNum=PassedPortNum;									// Store port number used (for freeing up resources in desconstructor)	
    
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);    // ESP32 will allocated resources to run I2S
    i2s_set_pin(PortNum, &pin_config);                      // Tell it the pins you will be using         
	
	InitSineValues();								// create our table of sine values. Speeds things up to if you pre-calculate
}

XT_I2S_Class::~XT_I2S_Class()
{
	i2s_driver_uninstall(PortNum);									//Free resources						
}


void XT_I2S_Class::Play(XT_PlayListItem_Class *Sound)
{
	Play(Sound,true,-1);				// Sounds mix by default
}

void XT_I2S_Class::Play(XT_PlayListItem_Class *Sound,bool Mix)
{
	Play(Sound,Mix,-1);				// Sounds mix by default
}

void XT_I2S_Class::Play(XT_PlayListItem_Class *Sound,bool Mix,int16_t TheVolume)
{

	// check if this sound is already playing, if so it is removed first and will be re-played
	// This limitation maybe removed in later version so that multiple versions of the same
	// sound can be played at once. Trying to do that now will corrupt the list of items to play
	// Volume of -1 will leave at default. As a work around just create a new sound object of you
	// want to play a sound more than once. For example if you want a laser sound to happen perhaps
	// many times as user presses fire, then just create a new instance of that sound first and pass to
	// this routine.

	if(AlreadyPlaying(Sound))
		RemoveFromPlayList(Sound);
	if(Mix==false)  							// stop all currently playing sounds and just have this one
		StopAllSounds();

	Sound->NewSound=true;						// Flags to fill buffer routine that this is brand new sound
												// with nothing yet put into buffer for playing
	Sound->RepeatIdx=Sound->Repeat;				// Initialise any repeats
	if(TheVolume>=0)
		Sound->Volume=TheVolume;

	// set up this sound to play, different types of sound may initialise differently
	Sound->Init();

	// add to list of currently playing items
	// create a new play list entry
	if(FirstPlayListItem==nullptr) // no items to play in list yet. TEB, Oct-10-2019
	{
		FirstPlayListItem=Sound;
		LastPlayListItem=Sound;
	}
	else{
		// add to end of list
		LastPlayListItem->NextItem=Sound;
		Sound->PreviousItem=LastPlayListItem;
		LastPlayListItem=Sound;

	}
	Sound->Playing=true;					// Will start it playing
}

void XT_I2S_Class::RemoveFromPlayList(XT_PlayListItem_Class *ItemToRemove)
{
	// removes a play item from the play list
	if(ItemToRemove->PreviousItem!=nullptr)   
		ItemToRemove->PreviousItem->NextItem=ItemToRemove->NextItem;
	else
		FirstPlayListItem=ItemToRemove->NextItem;
	if(ItemToRemove->NextItem!=nullptr)     
		ItemToRemove->NextItem->PreviousItem=ItemToRemove->PreviousItem;
	else
		LastPlayListItem=ItemToRemove->PreviousItem;

	ItemToRemove->PreviousItem=nullptr;       
	ItemToRemove->NextItem=nullptr;           

}

bool XT_I2S_Class::AlreadyPlaying(XT_PlayListItem_Class *Item)
{
	// returns true if sound already in list of items to play else false
	XT_PlayListItem_Class* PlayItem;
	PlayItem=FirstPlayListItem;
	while(PlayItem!=nullptr)           
	{
		if(PlayItem==Item)
			return true;
		PlayItem=PlayItem->NextItem;
	}
	return false;
}


void XT_I2S_Class::StopAllSounds()
{
	// stop all sounds and clear the play list

	XT_PlayListItem_Class* PlayItem;
	PlayItem=FirstPlayListItem;
	while(PlayItem!=nullptr)       
	{
		PlayItem->Playing=false;
		RemoveFromPlayList(PlayItem);
		PlayItem=FirstPlayListItem;
	}
	FirstPlayListItem=nullptr;    
}

int16_t CheckTopBottomedOut(int32_t Sample)
{
	// checks if sample is greate or less than would be allowed of it was a 16 but number, if so returns the max value
	// that wpuld be allowed for a signed 16 bt number
	if(Sample>32767)									
		return 32767;								    // Sound has topped out, return max
	if(Sample<-32768)
		return-32768;									// Sound has bottomed out, return max
	return Sample;									    // Sound is within range, return unchanged
}

uint32_t XT_I2S_Class::MixSamples()
{
	// Goes through all sounds and mixes the next sample together, returns the mixed stereo 16 bit sample (so 4 bytes)
    XT_PlayListItem_Class *PlayItem,*NextPlayItem;
	uint32_t MixedSample;	
	int16_t LeftSample,RightSample;			// returned samples
	int32_t LeftMix,RightMix;				// Bigger than 16bits as we are adding several samples together					

	PlayItem=FirstPlayListItem;
	LeftMix=0;									// Start with silence
	RightMix=0;									// Start with silence
	while(PlayItem!=nullptr)
	{
		if(PlayItem->Playing)						// If still playing
		{
			
			PlayItem->NextSample(&LeftSample,&RightSample);						// Get next byte from this sound to play
			if(PlayItem->Filter!=0) 
				PlayItem->Filter->FilterWave(&LeftSample,&RightSample);	// Adjust by any set filter, a Work in Progress
			// set any volume for this sample
			SetVolume(&LeftSample,&RightSample,PlayItem->Volume);
			LeftMix+=LeftSample;
			RightMix+=RightSample;
		}

		NextPlayItem=PlayItem->NextItem;					// move to next play item
		if(PlayItem->Playing==false)						// If this play item completed
		{
			if(PlayItem->RepeatForever)
			{
				PlayItem->Init();						// initialise for playing again
				PlayItem->Playing=true;
			}
			else
			{
				if(PlayItem->RepeatIdx>0)				// Repeat sound X amount of times
				{
					PlayItem->RepeatIdx--;
					PlayItem->Init();						// initialise for playing again
					PlayItem->Playing=true;
				}
				else
					RemoveFromPlayList(PlayItem);		// no repeats, remove from play list
			}
		}
		PlayItem=NextPlayItem;							// Set to next item
	}
	// master volume (volume of all output), have to convert to 16bit temporarily for the volume function
	int16_t Left=int16_t(LeftMix);
	int16_t Right=int16_t(RightMix);
	SetVolume(&Left,&Right,Volume);
	LeftMix=Left;							// Back into 32bit here as
	RightMix=Right;
	// Correct sample if topped or bottemed out (wave gone beyond max values for 16 bit), and ensure it's set to a 16 bit number
	// We had to work in 32 bits whilst mixing so we didn't potentially lose any resolution when mixing. But now need 16 bits
	// again before we combine and return the final 2 samples as 1 32bit value
	LeftMix=uint16_t(CheckTopBottomedOut(LeftMix));
	RightMix=uint16_t(CheckTopBottomedOut(RightMix));
	// move the 2 16bit samples into one 32bit variable
	MixedSample=(LeftMix<<16)|(RightMix);				
	return MixedSample;
}  


void XT_I2S_Class::FillBuffer()
{
    // Basically fill the I2S buffer with data. However as we want to mix sounds etc and a sound may have been playing for
    // a while when we add another sound (to mix) then we need somewhere where we can mix these sounds, as once sent to the 
    // I2S buffer it would be hard if not impossible to try to mix in those buffers. So we have two more buffers that we
    // have control over that we can use to mix in sounds as required. It's one of these buffers that we will be sending to
    // the I2S system, we will fill the I2S with one buffer as we mix sounds together into the other and then swap
    // periodically.
    
	uint32_t MixedSample;
  	size_t NumBytesWritten=4; // Set to 4 so loop enters below

	while(NumBytesWritten>0)  // keep filling until full, note of we run out of samples then MixSamples will return 0 (silence)
	{						  // i2s_write (below) should fill in 4 byte chunks, so will return 0 when full	
		MixedSample=MixSamples();
		// Store sample in buffer, increment buffer pointer, check for end of buffer or for end of sample data	
		// The 4 is number of bytes to write, it's 4 as 2 bytes per sample and stereo (2 samples)
		// 1 is max number of rtos ticks to wait
		i2s_write(I2S_NUM_0, &MixedSample, 4, &NumBytesWritten, 1);
	}	
}


void XT_I2S_Class::Beep()
{
	//Play as simple beep, 0.5s long,700Hz,half vol, sine wave wave form
	Beep(100,700,0.5,WAVE_SINE,true);
}

void XT_I2S_Class::Beep(uint32_t Duration)
{
	//Play as simple beep for duration length,700Hz,half vol, sine wave wave form
	Beep(Duration,700,0.5,WAVE_SINE,true);
}

void XT_I2S_Class::Beep(uint32_t Duration,int16_t Pitch)
{
	//Play as simple beep for duration length,Pitch (which can be a note as defined in the header file),half vol, sine wave wave form
	Beep(Duration,Pitch,0.5,WAVE_SINE,true);
}

void XT_I2S_Class::Beep(uint32_t Duration,int16_t Pitch,int16_t Volume)
{
	//Play as simple beep for duration length,Pitch (which can be a note as defined in the header file),half vol, sine wave wave form
	Beep(Duration,Pitch,Volume,WAVE_SINE,true);
}

void XT_I2S_Class::Beep(uint32_t Duration,int16_t Pitch,int16_t Volume,uint8_t WaveType)
{
	//Play as simple beep for duration length,Pitch (which can be a note as defined in the header file)
	// volume as passed, sine wave wave form
	Beep(Duration,Pitch,Volume,WaveType,true);
}

void XT_I2S_Class::Beep(uint32_t Duration,int16_t Pitch,int16_t Volume,uint8_t WaveType,bool Mix)
{
	// Play as simple beep for duration length,Pitch (which can be a note as defined in the header file)
	// volume as passed, WaveType as defined in header file
	
	if(Mix==false)  							// stop all currently playing sounds and just have this one
		StopAllSounds();
	XT_Instrument_Class* BeepInstrument=new XT_Instrument_Class(INSTRUMENT_NONE,Volume);
	BeepInstrument->SetWaveForm(WaveType);
	BeepInstrument->SetFrequency(Pitch);
	BeepInstrument->SetDuration(Duration);
	BeepInstrument->DumpData=true;
	Play(BeepInstrument);						// Play it
	
}