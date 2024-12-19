
/* [1.1.23]

  Keyer Rev 1.1
  
   5/7/2019 I'm starting to develop an Arduino keyer. I hope.
   I've already done a Morse file that sends code from characters. It was
   used by PHSNA and also by Expr_DDS.ino. I'll borrow some stuff from those,
   as well as the basic Morse.ino file. 
   The intent here is to expand to the use of paddles. Currently just wanting
   a simple keyer. Nothing too fancy. (Famous last words ...)
   
   This keyer is intended to be somewhat like my Atari keyer which I started 
   on 5/17/1987, almost exactly 32 years ago.
   
   5/10/2019 - Added interpreting char after it is send and putting to
               serial output. Eventually use for commands from paddle.
			   Added speed pot.
			   
   5/13/2019 - Added lockout and error tones if paddle stuck > 255 elements
   
   5/16/2019 - 1.0 Added a number of commands and features.
				Added save to EEPROM
				
   5/17/2019 - Got message save to EEPROM working. Also got message playback
               working.
			   Added PB1 and PB2 to lines D2 and D3. Also a PB3 will connect
			   to pull both down. PB3 will cause entry into command mode, 
			   since it will be the one-switch option.
			   
	5/20/2019	At this time I have my messages working and the ability to 
				embed commands into messages. This can be my first 
				version 1.0.
				
	5/22/2019	I've added many features including messages embedded within
				messages, compose messages on serial terminal, terminate
				by tapping a button, etc. My full diary on programming 
				progress is in my KeyerWA5BDU_info.docx document.
				
				A somewhat major change I'm trying now is to get away from
				the old "half space" allowance I had on the Atari. The idea
				was to allow a half space before sealing in the same element.
				But I wonder if being different from other keyers will make
				it feel odd to people not used to it.  There's probably no
				real "right way", just what you're used to.
				So currently I keep a little bit of what I had by going to 
				a fixed 10 ms allowance instead of half of a space. Note 
				that at 25 WPM, half a space was 24 ms.  See 6/10 for
				resolution of this issue.
	
	6/10/2019	Finished the keyer analyzer project and now back on the keyer.
				I changed the logic so I now only have to move the pot by
				3 WPM to re-enable it after disabling with a WPM entry by 
				command. Now I have to get rid of the explicit command to 
				re-enable, which is no longer needed. Done.
				
				I changed some timing variables from 8 to 16 bit so I can get
				down to 10 WPM. Changed speed limits to allow that.
				
				Changed the "pitch" steps from 36 Hz to 20 Hz.
				
				I changed the "time to get off the paddle" to 75% to agree
				somewhat with K3, KX3 and FT-991A. Nobody else uses 50%.
				
	6/12/2019	Added a function to control a T/R relay using pin D4. Boolean
				controlTR must be true for the function to work. Leave it 
				false if T/R switching is not being used.
				TXOnDelay parameter is in milliseconds between energizing the
				relay and closing the key line. So when first closing the key,
				there will be that delay before the TX is keyed.
				TXHold is the time that the relay is held closed after
				keying stops.
				TXHold is in units of dot lengths, so it varies with WPM. For 
				example, a value of 7 would be 7 x 60 or 420 ms at 20 WPM.
				
	8/22/2019	Now sends "ESE" or shave and a haircut after 15 minutes of no
				activity and repeat every 15 minutes. This is a reminder to 
				save the battery. There's a pitch shift to give it a unique
				sound.

	6/30/2020	Version 1.1 - back on it after ten months away. Got some feedback
	            from N5IB who gave it a workout on Field Day. He suggests an
				acknowledgment beep after writing EEPROM. Also, an Elecraft K2
				scheme where closing both paddles at exactly the same time is
				treated as a straight key closure. This would require diode
				logic to assure timing.
				Finally, I still need to fix the bug that requires a second
				press of a message button occasionally.
				The NewTone library has given me trouble in various projects
				by disappearing. So I've gone back to the almost identical
				"tone" library.
				I implemented the K2 both-paddles scheme as best I could but 
				operation was not consistent so I've commented it out for 
				now.
				*** I finally tracked down the "Moses bug" that sometimes
				required an extra tap to initiate a message. I found that the 
				MsgGetChar() routine would call Any() to see if the user
				wanted to cancel the message. Contact bounce when initiating
				the message could trigger Any() and cancel the message. I 
				changed it so it doesn't check on the 1st character and it
				delays 250 ms after message initiation. I also changed Any()
				so it waits until all paddle & button inputs are open before
				returning.
				*** Also as part of the V1.1a update, I added the ability to 
				have Message A repeat. A new command Lnn sets the delay between
				repeats as n.n seconds. I added this to the parameters stored
				& loaded via EEPROM and to the I information report.
				
				3/28/2024: A cosmetic edit: I had PB1Short, PB2Short and PB3Press 
				all meaning a brief tap. I confused  myself with the inconsistency 
				so I'm making all PB3Press appearances become PB3Short
   
   */
	// #include <NewTone.h>
	#include <EEPROM.h>
	
	
	#define LEDpin 13 // Pin 13 - on board LED
	#define CMDled 11 // Pin 11 show command mode, maybe just for troubleshooting
	#define spkrpin 7 // for sidetone
	#define KeyOut	8
	#define DotContact 5
	#define DashContact 6
	#define PB1 2
	#define PB2 3
	#define TX_RX 4 // T/R relay control
	#define PTT_IN A6 // T/R control via external input
	#define POT_ADC A1	
	#define WPM 22
	#define SPD_LIMIT_LO 10.0 //
	#define SPD_LIMIT_HI 45.0
	#define CMND_SPEED 22

// define some EEPROM addresses

	#define E_speed 0 // 8 bit fields except as noted
	#define E_pitch 1 // 16 bit unsigned int
	#define E_keyDNstate 3
	#define E_SpeedPot 4
	#define E_ModeA 5
	#define E_SideTone 6
	#define E_EE_CODE 7 // this holds unit16_t. 
	#define E_TR_RELAY 9 // holds true/false of controlTR
	#define E_MsgRptTime 10 // 0 to 99 tenths of second, 0 is repeat off V1.1a
	#define E_MEM_A 25 // Four 75 character memories
	#define E_MEM_B 100
	#define E_MEM_C	175
	#define E_MEM_D 250
	
	
	#define REV_TEXT "REV 1.1"
	
	// Next available EEPROM memory 250 + 75 = 325
	
	#define EE_CODE 19488 // save to EEPROM to flag EEPROM written/valid
	                      // comes from 1948, August

	uint16_t ditlen, dahlen, halfspace; 
	uint8_t speed, speed_old;
	char SpeedString[] = "00 WPM"; // text equivalent of speed
	uint16_t wordspace;
	
	// **USER** eight variables below
	
	unsigned int pitch = 600; // local peak for CEM-1201(50)
	char PitchString[] = "600 HZ"; // changed by program when pitch is changed
	uint8_t keyDNstate = LOW;
	uint8_t keyUPstate = HIGH;
	uint8_t TX_RX_Rstate = LOW; // line state while NOT in transmit
	uint8_t TX_RX_Tstate = HIGH; // keep these two negative of each other
	bool controlTR = false; 
	bool OnIdleAlarm = true; // make 'false' for no "power on" reminder
	
	
	bool TRrelayON = false;
	bool TRbyPTTON = false; // T/R energized because PTT grounded
	
	// **USER** two variables below
	
	uint8_t TXOnDelay = 15; // milliseconds from TX relay to close key
	uint16_t TXHold = 11; // time in Morse space units until drop out T/R 
	
	
	bool DitLatch = false; 
	bool DahLatch = false;
	bool SpLatchDit = false;
	bool SpLatchDah = false;
	bool DoTransmit = true; // "true" keys TX on keydown, "false" tone only
	bool DoTransmitOld = true; // save old while primary temporarily altered
	uint8_t buildchar = 1; // where dots and dashes assemble into a char
	uint8_t newchar = 0; // buildchar moved here when done. 0 means not done
	uint8_t i = 0; // counter
	bool char_done = true; // True when char sent by user is finished
	uint8_t DeadMan = 255; // Count for stuck key shut down feature
	bool DoLockout = false; // flag that lockout condition exists
	bool CMNDMode = false; 
	bool SpeedPot = true; // Do use speed pot to set speed
	int staticPot; // save current reading when going to disable mode
	uint8_t CMNDChar = 'Z'; // CMNDChar is 1st letter of cmnd seq. Z = none
	uint8_t CMND_SEQ[4]; // Here cmnd seq chars after CMNDChar are accumulated
	uint8_t CMND_SEQ_ptr = 0;
	uint8_t XChar; // char translated from newchar to ascii
	
	// **USER** two variables below
	
	bool ModeA = false; // true for Mode A, false for Mode B
	bool SideTone = true; // true means DO generate sidetone
	
	bool SideToneOld = true; // place to store current while changing
	uint16_t ee_code = 0; // Flag EEPROM has been written when = 19488
	uint8_t MessagePtr = 0;
	uint8_t MessageID = 0; // Will hold A, B, C or D for current message or
						   // 0 (zero) if not currently recording/sending
	uint16_t MsgAdr = 25; // Specific EEPROM base address for current message
	uint16_t MsgAdrSave; // Place to save when message calls message
	uint8_t MsgBuffer[76]; // place to keep message before saving
	uint8_t msgchar = 1; // char taken from EEPROM. If 0, end of message
	uint8_t msgcountchar = 0; // pointer into EEPROM message memory
	uint8_t msgcountcharSave; // Place to save counter when msg calls msg
	bool MsgActive = false; // to tell main code if we are sending message
	bool Record = false; // True when in recording mode
	uint8_t MsgRptTime = 0; // V1.1a - time is in tenths of a second
	char MsgRptTimeStr[] = "0.0";  // V1.1a
	
	bool PB1Short = false;
	bool PB2Short = false;
	bool PB1Long = false;
	bool PB2Long = false;
	bool PB3Short = false;
	bool PB3Long = false;
	
	uint8_t misc_count = 0;
	uint8_t loopCount = 0; // count passes through main loop
	
	unsigned long timing_start;
	unsigned long timing_start_2; // for timing of button press
	unsigned long timingTXHold; // time to drop out T/R relay
	unsigned long battTimer; // timer for reminder that keyer is ON
	// bool testFlag = true; // *******  TEMPORARY ************
	
	String SerialMsg = "";
	
// **************  TABLE OF ASCII CODES IN MORSE ORDER ********************

// Table below borrowed from my Atari/6502 code for the WA5BDU Keyer
// The idea is to assemble a Morse code in dots and dashes and use that as
// an index to look up the equivalent ASCII character. Starts with zero.PB3Short

// 3/20/2024 just adding this comment. This table isn't the "inverse" of
// the ASCII to Morse lookup table, because in this case the index is
// created by starting with a 0000 0001 character and left-shifting in
// a 1 for each dash and 0 for each dot. So you see, letter 'A' is the
// 5th element because it came from 0000 0101. In the morse[] table,
// 'A' is 0110 0000 because the element starts on the left (MSB) and moves
// right until a '1' is encountered.

	byte backmorse[128] = {32, 32, 'E', 'T', 'I', 'A', 'N', 'M', 'S', 'U', // 9
	                     'R', 'W', 'D', 'K', 'G', 'O', 'H', 'V', 'F', ' ', // 19
					'L', 10, 'P', 'J', 'B', 'X', 'C', 'Y', 'Z', 'Q', ' ', // 30
					8, '5', '4', 32, '3', 32, 32, 32, '2', '*', 32, 32, 32, // 43
					32, 32, 32, '1', '6', '=', '/', 32, 32, '!', 32, 32, // 55
					'7', 32, 32, 0,'8', 32, '9', '0', 32, 32, 32, 32, 32, // 68
					32, 32, 32, 32, 32, 32, 32, '?', 32, 32, 32, 32, 32, 34, // 82
					32, 32, '.', 32, 32, 32, 32, '@', 32, 32, 32, 39, 27, // 95
					32, 32, 32, 32, 32, 32, 32, 32, 32, 32, ';', 32, 32, 32,
					32, 32, 32, 32, 32, ',', 32, 32, 32, ':', 32, 32, 32, 32,
					32, 32, 32, 32,};
	
	/*
		32 is space
		 9 is backspace
		34 is " 
		39 is '
		27 is ESC, translated from .----- as in EOM for end-of-messages
		Code for 90 is [AC] for 'AT' sign: @
		Code 21 [AA] gives 10 or linefeed. Gives newline on Arduino monitor
		Code 40 [AS] gives * asterisk, meaning enter command mode
		Code 95 translates to ASCII 27 or Escape. I use for End Of Message since
		[EOM] sent as one char gives 95.
		Code 59 for [QT] or 'quit" translates to 0 which I use for "cancel message".
		
	*/
		 
	
	//void morseSetup(void); // declaration
	void morseSetSpeed(); // declaration V1.1

//
// **********************************************************************
//                                                                      *
//                                S E T U P                             *
//                                                                      *
// **********************************************************************
   
   void setup()
   {
	   	speed = WPM;
		// morseSetup calcs ditlen and dahlen
	   	morseSetSpeed(); 
		pinMode(DotContact, INPUT_PULLUP);
		pinMode(DashContact, INPUT_PULLUP);
		pinMode(PB1, INPUT_PULLUP);
		pinMode(PB2, INPUT_PULLUP); 
		pinMode(KeyOut, OUTPUT);
		pinMode(spkrpin, OUTPUT);
		pinMode(LEDpin, OUTPUT);
		pinMode(CMDled, OUTPUT);
		pinMode(TX_RX, OUTPUT);
		pinMode(PTT_IN, INPUT_PULLUP); // external control of T/R relay
		analogReference(DEFAULT); // Use +5 volt Vcc for reference

		Serial.begin(9600);
		Serial.println("WA5BDU Keyer");
		Serial.println(REV_TEXT);
		
		digitalWrite(LEDpin, LOW);
		digitalWrite(CMDled, LOW); // NOT in command mode
		digitalWrite(TX_RX, TX_RX_Rstate);
		
		readSpeedPot(); 
		Serial.write("SPEED: ");
		Serial.println(speed, DEC);
		
		speed_old = speed;
		
		MsgsToSerial();

	// V1.1a change so both paddles held closed on startup bypasses load
	// from EEPROM
		
	//	EESave(); // one-time, then comment out
	
		if(EE_written() && !(!digitalRead(DotContact) &&
			!digitalRead(DashContact)))
		{
		EERecall(); // Load keyer setup from EEPROM
		}
		else 
		{	
		Serial.println(" EEPROM config not found or cancelled");
		rasp(); // V1.1a let user know EE read has been skipped
		}
		
		digitalWrite(KeyOut, keyUPstate);
		
		DoTransmit = false;
		morseSendString("TU");
		DoTransmit = true;
		battTimer = millis(); // initialize POWER ON reminder timer
		
		// *************  TEMPORARY *****************
	/*	
		SideToneOld = SideTone;
				SideTone = true; // do send sidetone
				DoTransmitOld = DoTransmit;
				DoTransmit = false; // don't send on the air
				send_code('E');
				pitch +=10;
				send_code('S');
				pitch -=10;
				send_code('E');
				DoTransmit = DoTransmitOld; // restore previous
				battTimer = millis();
				SideTone = SideToneOld;
				
				delay(5000);
	*/
				
   }
   
   
// **********************************************************************************
//                                                                                  *
//                              M A I N   L O O P                                   *
//                                                                                  *
// **********************************************************************************

	void loop()
	{
		
		/*
		I want to time the loop with nothing going on and display
		the time for my info. I'll do 100 passes and print the total.
		OK,I did the timing and the values started with 6 ms, then alternated
		3 and 4 and then finally settled on 2 & 3 ms alternating.
		
		So my worst case is 0.06 ms or 60 us and typical about 30 us per
		trip through the loop with nothing going on. Note that the time to
		do the first serial print is included in the time.

		(Uncomment below lines to do timing)

		if(!misc_count) timing_start_2 = millis();
		
		misc_count++;
		if(misc_count == 100)
		{
			Serial.print("Time: ");
			Serial.println((millis() -  timing_start_2), DEC);
			delay(1500);
		}
		*/
		
		// V1.1 - interpret both paddles closed as hand key closed if nothing
		// else his happening now ...
		
		/*
		if(!CMNDMode && char_done)
		{
			bool didclose = false;
			if(HandKeyK2()) // true if both paddles closed
			{
				if(DoTransmit) digitalWrite(KeyOut, keyDNstate); 
				if(SideTone) tone(spkrpin, pitch); // produce tone
				didclose = true;
			}
				while(HandKeyK2()); // Stay until paddle(s) open				
		
			if(didclose)
			{
				digitalWrite(KeyOut, keyUPstate); // open keyed line
				if(SideTone) noTone(spkrpin); // stop tone
			}
		}
		*/
			
		loopCount++; // Count number of passes through main loop, 255 max
		
		if(!SpeedPot && !CMNDMode && !loopCount)
		{
			freeSpeedPot(); // if pot moved >= 3 WPM re-enable
			
							
		}
		
		
		if(!loopCount)
		{
			// I think once per 256 trips is enough for PTT checks too
			if(controlTR)
			{
				if(!digitalRead(PTT_IN)) // if PTT low (grounded)
				{
					if(!TRbyPTTON) // energize PTT, if not already
					{
					digitalWrite(TX_RX, TX_RX_Tstate);
					TRbyPTTON = true;
					}
				}
				else // PTT is found to be high
				{
					if(TRbyPTTON) // de-energize PTT, if not already
					{
					digitalWrite(TX_RX, TX_RX_Rstate);
					TRbyPTTON = false;
					}
				}
			}
			
			// Also limit to once per 256 loops, check of time keyer has
			// been on without user action so it can beep a reminder after
			// 15 minutes or so.
			
			if(((millis() - battTimer) > 900000UL) && OnIdleAlarm ) // 15 minutes w/o action
			// 900,000 milliseconds = 15 minutes
			{
				SideToneOld = SideTone;
				SideTone = true; // do send sidetone
				DoTransmitOld = DoTransmit;
				DoTransmit = false; // don't send on the air
				send_code('E');
				pitch +=15;
				send_code('S');
				pitch -=15;
				send_code('E');
				DoTransmit = DoTransmitOld; // restore previous
				battTimer = millis();
				SideTone = SideToneOld;
				
			}
							
		}
		
		
		if(SpeedPot && !CMNDMode && !loopCount)
		{
			readSpeedPot(); // Every 256 times through main loop ~ 10 ms
		}
		
		if(Record && AnyButton())
		{
			XChar = 27;
			Recording();
		}
		// A latched space has priority over also latched dit or dah
		
	// See if we are sending a message. If so, get a char and go to
	// ProcessChar() and treat it as though sent from the paddle
	

		XChar = ' ';
		while((XChar == ' ') && MsgActive) // reject any returned spaces
		{	
		if(MsgActive) MsgGetChar(); // Gets to XChar, sets MsgActive false
									// if XChar = 0. Sends if ordinary char
		}
		
		if(MsgActive && (CMNDMode || XChar == '*')) ProcessChar();

		// Below is the place in the main loop where we actually read the
		// paddles and create a dot or dash if demanded
	
		checkPaddles();
		if(SpLatchDit) PspaceDit();
		if(SpLatchDah) PspaceDah();		
			if(DitLatch) Pdit();
		if(SpLatchDah) PspaceDah();
		if(SpLatchDit) PspaceDit();		
			if(DahLatch) Pdah();

	// Within the flow of this main loop, I'm pretty sure no sending of
	// Morse elements or spaces is going on. So I can see if my T/R relay
	// is energized and if so, check the timing for drop-out. Timing gets
	// reset each time an inter-character space is detected. And each
	// element generated.
	
	if(TRrelayON)
	{
		if(millis() >= (timingTXHold + (TXHold * ditlen)))
		{
			digitalWrite(TX_RX, TX_RX_Rstate);
			TRrelayON = false;
		}
	}



	if(!Record) // When recording, buttons used for [EOM]
		{
		if(GetButtons())
		{
			if(PB3Short)
			{
				newchar = 40; // simulate[AS] which maps to '*' for CMND mode
				PB3Short = false;
			}
			if(PB1Short || PB3Long)
			{
				CMNDChar = 'M'; // PB1 short will send message A
				CMNDMode = true;
				digitalWrite(CMDled, HIGH);
				newchar = 5; // simulate 'A' was sent by paddle
				PB1Short = false;
				PB3Long = false;
			}
			if(PB1Long)
			{
				CMNDChar = 'M'; // PB1 long will send message B
				CMNDMode = true;
				digitalWrite(CMDled, HIGH);
				newchar = 24; // simulate 'B' was sent by paddle
				PB1Long = false;
			}
			if(PB2Short)
			{
				CMNDChar = 'M'; // PB2 short will send message C
				CMNDMode = true;
				digitalWrite(CMDled, HIGH);
				newchar = 26; // simulate 'C' was sent by paddle
				PB2Short = false;
			}
			if(PB2Long)
			{
				CMNDChar = 'M'; // PB2 long will send message D
				CMNDMode = true;
				digitalWrite(CMDled, HIGH);
				newchar = 12; // simulate 'D' was sent by paddle
				PB2Long = false;
			}
		}	
		}		
		//got_new:;
		
		//checkPaddles();		
		if(newchar != 0) 
		{
			Serial.write(backmorse[newchar]);
			ProcessChar();
			//Serial.print(newchar, BIN);
			//Serial.println();
			newchar = 0;
			
			// Now, end of character has been detected. If six more spaces
			// occur with no paddle action, that's a word space. Note that
			// timing_start hasn't been reset yet ...
			
			while((millis() - timing_start) <= wordspace) 
			{
				newchar = 1;	// 1 will map to ASCII space   
				if(!digitalRead(DotContact) || !digitalRead(DashContact))
				{
					newchar = 0; // paddle action? Quit loop and set
					goto beyond; // newchar to 0
				}
			}
			beyond:;
		}
		
	if(DoLockout) LockOut();
		
}	
// ******************  END OF MAIN LOOP ***********************************


	
	void Pdit()
	{
		char_done = false;
		if(controlTR && !TRrelayON) closeTR(); // close T/R relay if rqd 
		timing_start = millis();
		
		if(DoTransmit) digitalWrite(KeyOut, keyDNstate); 
		if(SideTone) tone(spkrpin, pitch); // produce tone
		DitLatch = false; // clear if was latched
		digitalWrite(LEDpin, HIGH);
		buildchar = buildchar << 1; // shift in 0 for a dit
		DeadMan--;
		
		// Now while sending dit, if dash paddle closes, latch it
		
		while( (millis() -  timing_start) <= ditlen)
		{
			if(digitalRead(DashContact)==LOW) DahLatch = true;
		}
		
		// Now dit has timed out, dash may or may not be latched, time for
		// a space ...
		
		SpLatchDit = true;
		if(ModeA) DahLatch == false;
		
		timingTXHold = millis(); // reset with each element generated
		
	}


	void Pdah()
	{
		char_done = false;
		if(controlTR && !TRrelayON) closeTR(); // close T/R relay if rqd 
		timing_start = millis();
		if(DoTransmit) digitalWrite(KeyOut, keyDNstate); // close keyed line
		if(SideTone) tone(spkrpin, pitch); // produce tone
		digitalWrite(LEDpin, HIGH);
		DahLatch = false; // clear if was latched
		buildchar = (buildchar << 1) + 1; // shift in 1 for dash
		DeadMan--;
		
		// Now while sending dash, if dit paddle closes, latch it
		
		while( (millis() -  timing_start) <= dahlen)
		{
			if(digitalRead(DotContact)==LOW) DitLatch = true;
		}
		
		// Now dash has timed out, dit may or may not be latched, time for
		// a space ...
		
		SpLatchDah = true;
		if(ModeA) DitLatch = false;
		timingTXHold = millis(); // reset with each element generated
	}

// Creating separate routines for space following dot and space following
// dash. This is so I can delay latching same element but allow opposite
// element.



// **************  SPACE FOLLOWING DOT ELEMENT ****************************


	void PspaceDit()
	{
		timing_start = millis();
		digitalWrite(KeyOut, keyUPstate); // open keyed line
		if(SideTone) noTone(spkrpin); // stop tone
		SpLatchDit = false; // clear if was latched
		digitalWrite(LEDpin, LOW);
		if(DeadMan == 0) DoLockout = true;
		
		// The purpose of the delay below is to prevent latching a new
		// element at the very start of the space period. The user needs
		// a little time to get his finger off the paddle, at least I do.
		// This is what I did with my Atari keyer way back around 1987 or
		// so.
		// As of 6/10/2019, I'm trying 'halfspace' equal to about 75% of
		// a space, mimicking what the K3, KX3 and FT-991A do.
		
	while( (millis() -  timing_start) <= halfspace)
		
		{
			// in this 1st half of space, I allow latching dash but not dot
			
			if(digitalRead(DashContact)==LOW) DahLatch = true;
		}
		
		// In 2nd half of space, either ele can latch if nothing latched yet
		// Since I haven't reset timing_start, I can continue to count to
		// full ditlen
		
	while( (millis() -  timing_start) <= ditlen)
		
		{		
			checkPaddles();
		}	

	// Now space has completed, new ele may or may not be latched
	// If nothing is latched, current char is finished
	// This is also where I'll start timing for T/R relay dropout
	// if we are doing T/R switching
	
	if(!DahLatch && !DitLatch)
		{
			newchar = buildchar; // save for whatever use
			buildchar = 1; // ready for new build and flag done
			DeadMan = 255; // reset count whenever both paddles open
			char_done = true;
			//if(controlTR) timingTXHold = millis() + ditlen;
		}
		
		timingTXHold = millis(); // reset with each space generated
	
	}

// ************  SPACE FOLLOWING DASH ELEMENT *****************************
	
	void PspaceDah()
	{
		timing_start = millis();
		digitalWrite(KeyOut, keyUPstate); // open keyed line
		if(SideTone) noTone(spkrpin); // stop tone
		SpLatchDah = false; // clear if was latched
		digitalWrite(LEDpin, LOW);
		if(DeadMan == 0) DoLockout = true;		
		// The purpose of the delay [same as above comment]
		
	while( (millis() -  timing_start) <= halfspace)
		
		{
			// in this 1st half of space, I allow latching dot but not dash
			
			if(digitalRead(DotContact)==LOW) DitLatch = true;
		}
		
		// While doing 2nd half of space, either ele can latch if no latch
		
	while( (millis() -  timing_start) <= ditlen)
		
		{
			
			checkPaddles();
		}	

	// Now space has completed, new ele may or may not be latched
	
	// If nothing is latched, current char is finished
	// This is also where I'll start timing for T/R relay dropout
	// if we are doing T/R switching	
	
	if(!DahLatch && !DitLatch)
		{
			newchar = buildchar; // save for whatever use
			buildchar = 1; // ready for new build and flag done
			DeadMan = 255; // reset count whenever both paddles open
			char_done = true;
			//if(controlTR) timingTXHold = millis() + ditlen;
		}
	
		timingTXHold = millis(); // reset with each space generated
	}

// ****** CLOSE T/R RELAY AND DELAY RQD TIME BEFORE RETURN ****************

	void closeTR()
	{
		digitalWrite(TX_RX, TX_RX_Tstate); // energize relay
		timing_start = millis();
		while(millis() < (timing_start + TXOnDelay));
		TRrelayON = true;
	}

// *************  READ PADDLES ********************************************

	
	void checkPaddles()
	{
		// V1.1 below, exit if both paddles closed. I think I can get away
		// with this because in iambic mode I don't check for both paddles,
		// I just check for the opposite paddle. And not using this routine.
		//
		// K2 hand key mode didn't work well so I'm commenting it out
		
		/*
		
		if(!((digitalRead(DotContact)==LOW) || (digitalRead(DashContact)==LOW)))
		{
			delayMicroseconds(100);
		}
		
		if(!((digitalRead(DotContact)==LOW) && (digitalRead(DashContact)==LOW)))
		{
			*/
		if(!DitLatch && !DahLatch)
		{
			if(digitalRead(DotContact)==LOW) 
			{
				DitLatch = true; // 'P' distinguish from dit and

			}
			else 
			{
				if(digitalRead(DashContact)==LOW) DahLatch = true; // dah of character sender
			}
			
			if(DitLatch || DahLatch) battTimer = millis(); // reset  timer

		}
		//}
		
	}

// ***************** HAND KEY VIA BOTH PADDLES ******************************

// V1.1 The idea here is that if both paddles are closed simultaneously it
// means the user is using a straight key and diode logic to create the
// effect. This possibility will only be checked when the sending unit
// is not active and "nothing is going on". Key closed actions will be
// initiated and stay that way until the condition no longer exists.

/*
	bool HandKeyK2()
	{
		bool Hkey = false;
		if(digitalRead(DashContact)==LOW 			
			&& digitalRead(DotContact)==LOW) Hkey = true;
		return Hkey;
	}
	
	*/
	

// ************ READ STATUS OF PUSHBUTTON SWITCHES ************************

// This routine will return false if neither switch is closed. If a switch
// is closed, It checks for six possible statuses. PB1 short or long close,
// PB2 short or long close, and PB3 short or long close.
// In Elecraft terminology, these actions are called "Tap" and "Hold".

// Note || operator is Boolean (logical) OR 

	bool GetButtons()
	{
		bool rvalue = false; // means no info, no PB closed
		bool beep1 = false; // latches to limit to one beep
		bool beep2 = false; // latches to limit to one doublebeep
		
		if(digitalRead(PB1) && digitalRead(PB2)) goto gohome; // no buttons
		
		rvalue = true; // at least something was closed, flag it
		battTimer = millis(); // reset inactivity timer
		
		delay(10); // delay 10 ms and see if both are closed
		
		timing_start_2 = millis();	// measure how long button held closed
		
		while(!digitalRead(PB1) && !digitalRead(PB2)) // Here checking PB3
		{
			if(millis() > timing_start_2 + 500)
			{
				PB3Long = true;
				PB3Short = false;
				if(!beep2) DoubleBeep();
				beep2 = true;
			}
			else
			{
				PB3Short = true;
				if(!beep1) SingleBeep();
				beep1 = true;
			}
						
		}
		
		
		if(PB3Long || PB3Short) goto waitforopen; // only latch one
		
		// Wasn't PB3, so check for PB1		
		
		while(!digitalRead(PB1))
		{
			if(millis() > timing_start_2 + 500)
			{
				PB1Long = true;
				PB1Short = false;
				if(!beep2) DoubleBeep();
				beep2 = true;
			}
			else
			{
				PB1Short = true;
				if(!beep1) SingleBeep();
				beep1 = true;
			}
			//goto waitforopen;
		}
		
		if(PB1Long || PB1Short) goto waitforopen; // can only latch one boolean
		
	// Now we check for PB2 short or long
	
	while(!digitalRead(PB2))
		{
			if(millis() > timing_start_2 + 500)
			{
				PB2Long = true;
				PB2Short = false;
				if(!beep2) DoubleBeep();
				beep2 = true;
			}
			else
			{
				PB2Short = true;
				if(!beep1) SingleBeep();
				beep1 = true;
			}
			//goto waitforopen;
		}	
		
		waitforopen:;
		
		while(!digitalRead(PB1) || !digitalRead(PB2)); // Stay until both open
		
		gohome:;
		return rvalue;
		
	}

/* verify buttons work as expected

	void TestButtons()
	{
		if(GetButtons())
		{
			if(PB1Long) Serial.println("PB1Long");
			if(PB2Long) Serial.println("PB2Long");
			if(PB1Short) Serial.println("PB1Short");
			if(PB2Short) Serial.println("PB2Short");
			if(PB3Short) Serial.println("PB3Short");
			if(PB3Long) Serial.println("PB3Long");
			PB1Long = false;
			PB2Long = false;
			PB1Short = false;
			PB2Short = false;
			PB3Short = false;
			PB3Long = false;
		
		}
	}
*/
			
// *********  PROCESS COMMAND CHARACTERS INPUT VIA PADDLE ******************

// Only one character can be handled per pass through this routine since it 
// must return to the main loop to allow more paddle input.
// The first pass looks for the [AS] or '*' char and flags CMNDMode active
// if it were not previously.
// On entry, the global 'newchar' has been updated. It us used to index into
// backmorse[] to get the equivalent ASCII character. It is assigned to XChar
// If XChar is the first char received after '*', it becomes CMNDChar.
// CMNDChar is 'Z' when a command has not been initiated yet.


	void ProcessChar()
	{
	bool changed = false; // Used in TX Line and ...
	
		if(MsgActive) goto gotX; // have XChar from message send routine
		
		XChar = backmorse[newchar];
		
		
		// Below, if we are recording a message, go directly to that process
		// Message ID will contain 0 when not yet recording
		
		if(CMNDMode && (CMNDChar == 'R') && MessageID)
		{		
			Record = true;
			Recording();
			goto checkout;
		}
		
		gotX:;		
		if(XChar == ' ') goto checkout; // ignore space character
		
			
		if(!CMNDMode && (XChar != '*')) goto checkout; // no CMND MODE
		if(!CMNDMode && (XChar == '*')) // enter CMND MODE
		{
			CMNDMode = true;
			CMNDChar = 'Z';
			digitalWrite(CMDled, HIGH);
			SideToneOld = SideTone;
			SideTone = true;
			if(!MsgActive)
			{
			delay(125); // DoubleBeep comes too fast ...
			DoubleBeep(); // acknowledge in command mode
			}
			DoTransmitOld = DoTransmit; // save so can restore
			DoTransmit = false;
			if(speed > CMND_SPEED)
			{
				speed_old = speed;
				speed = CMND_SPEED; // command speed, slow it down
				morseSetSpeed();
			}
			goto checkout;
		}
	
	// Above code mainly concerned with finding '*' and getting into 
	// command mode. 
		
	// Below,keyer is in CMNDMode or should be, so check to see if first
	// char called CMNDChar has been received. If not, the current char
	// becomes CMNDChar. Commands that need more characters will exit and
	// the next time(s) through will get those chars. Commands that execute
	// after a single character (the CMNDChar) can do their action right here
	// (might call a function) and finish it. Those commands are H, A, B,
	// & P. Commands that need 1 or more additional chars are S, T, M, R.
	
		if(CMNDMode && (CMNDChar == 'Z')) // receiving 1st char of cmnd seq
		{
			CMNDChar = XChar;
			if(XChar == 'S') // Speed entry command
			{
				CMND_SEQ_ptr = 0;
				//MessageID = 0;
				goto checkout;

			}
			if(XChar == 'L') // Message repeat delay time entry command V1.1a
			{
				CMND_SEQ_ptr = 0;
				//MessageID = 0;
				goto checkout;

			}
			if(CMNDChar == 'R') // Record message command code
			{
				MessageID = 0;
				MessagePtr = 0;
				goto checkout;
			}
			if(CMNDChar == 'M') // Send a recorded message
			{
				MessageID = 0;
				MessagePtr = 0;
				goto checkout;
			}	
			
			if(XChar == 'H') HandKey();
			
			if(XChar == 'A')
			{
				ModeA = true;
				morseSendString(" OK A");
				Serial.println("\nMode A");
				goto goodexit;
			}
			if(XChar == 'B')
			{
				ModeA = false;
				morseSendString(" OK B");
				Serial.println("\nMode B");
				goto goodexit;
			}
			
			if(XChar == 'C') // Program message from 'C'OM port(serial)
			{
				MsgFromSerial();
				goto goodexit;
			}
			
			if(XChar == 'P') // Adjust pitch
			{
				PitchSet();
				Serial.println("\nPitch: ");
				Serial.print(pitch, DEC);
				goto goodexit;
			}
			
			if(XChar == 'Q') // Sidetone ON/OFF toggle
			{
				SideToneOld = !SideToneOld; // current option stored in 'OLD'
				delay(400);
				send_code('R');
				if(SideToneOld) Serial.println("\nSidetone ON");
				else Serial.println("\nSidetone OFF");
				goto goodexit;
			}
			if(XChar == 'I') // Information request
			{
				SendInfo();
				goto goodexit;
			}
			if(XChar == 'K') // toggle T/R control function
			{
				controlTR = !controlTR;
				SayTRState();
				goto goodexit;
			}
			
			if(XChar == 'V') // Save current setup to EEPROM
			{
				EESave();
				goto goodexit;
			}
			
			if(CMNDChar == 'T')
			{
				goto checkout;			
			}
			
			rasp(); //No valid entry
			
			goodexit:;
			ExitCMND(); // invalid entry cancels cmnd mode
		}
		
		// Above code was for capturing the first (or sometimes) only 
		// command code. For multi-char commands, we look below for
		// additional character(s).
		
		// Next, we process S for Speed command. After 'S', we accumulate
		// two numeric characters
		// Note at this point I know we are in CMND mode and valid CMNDChar
		// has been accepted
		
		
		if(CMNDChar == 'S')
		{
			
			// At this point I expect to receive two numeric digits
			// Note that if the user sends 'P' for either digit we cancel
			// the speed change and go back to using the speed pot. 
			// A 'Q' also cancels the function.
			
			/* No longer needed - now we automatically re-enable pot
			
			if(XChar == 'P')
			{
				SpeedPot = true;
				ExitCMND();
				// rasp();
				goto checkout;
			}
			*/ 
			
			if(XChar == 'Q') // 'Q' is quit cmnd to exit speed after starting
			{
				ExitCMND();
				
				rasp();
				goto checkout;
			}
			
			if(CheckNumeric(XChar))
			{
				CMND_SEQ[CMND_SEQ_ptr] = XChar;
				CMND_SEQ_ptr++;
				if(CMND_SEQ_ptr == 2) 
				{
					DoSpeedChange(); // I know I've captured two numerals
					morseSetSpeed();
					delay(200);
					if(!MsgActive) send_code('R'); // acknowledge good result
					ExitCMND();
				}
			}
			else // here, numeric check failed so try again
			{
				if(!MsgActive) bad_sound(); // V1.1a - don't play in message
			}
		} // End of SPEED logic
		
		

 // V1.1a Get loop time for message repeat. This is copied from the code
 // above which gets the speed, and modified for time
 
 
 
 		if(CMNDChar == 'L')
		{
			
			// At this point I expect to receive two numeric digits
			// A 'Q' also cancels the function.
			
			
			if(XChar == 'Q') // 'Q' is quit cmnd to exit  after starting
			{
				ExitCMND();
				
				rasp();
				goto checkout;
			}
			
			if(CheckNumeric(XChar))
			{
				CMND_SEQ[CMND_SEQ_ptr] = XChar;
				CMND_SEQ_ptr++;
				if(CMND_SEQ_ptr == 2) 
				{
					// I know I've captured two numerals
					MsgRptTime =  (CMND_SEQ[0] - '0') * 10; // seconds digit
					MsgRptTime += (CMND_SEQ[1] - '0'); // s/10 digit
					delay(200);
					if(!MsgActive) send_code('R'); // acknowledge good result
					ExitCMND();
				}
			}
			else // here, numeric check failed so try again
			{
				bad_sound();
			}
		} 
		
// End of Message repeat time input logic


// Below we see if we've received the T command for TX Line Logic. If so
// we look at the current character for 'L' (LOW) or 'H' (HIGH) for the
// key down state of the TX line.
		
		if(CMNDChar == 'T')
		{
			changed = false;
			if(XChar == 'L')
			{
				keyDNstate = LOW;
				keyUPstate = HIGH;
				changed = true;
				morseSendString(" TX LO");
				Serial.println("\nTX logic LOW");
			}
			if(XChar == 'H')
			{
				keyDNstate = HIGH;
				keyUPstate = LOW;
				changed = true;
				morseSendString(" TX HI");
				Serial.println("\nTX logic HIGH");
			}
				
			if(!changed)
			{
				bad_sound();
			}
			
			ExitCMND();
		}		
	
	// Here I'll look for the second char when I'm known to be in the 'R'
	// mode. R means record message and the 2nd char will be the message ID
	// of A - D.
		
		if(CMNDChar == 'R')
		{
			//Serial.println("Made it to R, etc.");
		// Current char must be in range A through D
		if((XChar < 'A') || (XChar > 'D'))
			{
				ExitCMND();
				//Serial.println("\nDidn't pass A-D");
			}
		else
			{
				MessageID = XChar; // Here known to be recording msg
				if(XChar == 'A') MsgAdr = E_MEM_A;
				if(XChar == 'B') MsgAdr = E_MEM_B;
				if(XChar == 'C') MsgAdr = E_MEM_C;
				if(XChar == 'D') MsgAdr = E_MEM_D;
			}
		}
		
		// Next, see if I'm in the 'M' mode for send message and get the 
		// message ID if so and start sending.
	
		if(CMNDChar == 'M') // && !MsgActive)
		{
		// Current char must be in range A through D
		
		if((XChar < 'A') || (XChar > 'D'))
			{
				ExitCMND();
			}
		else
			{
				
				MsgStart(XChar); // gets start address and sets MsgActive
			}
		}
		
		checkout:;
	}

// ***************  END OF PROCESS CHAR ROUTINE ***************************

	
	void ExitCMND()
	{
		CMND_SEQ_ptr = 0;
		CMNDChar = 'Z';
		CMNDMode = false;
		digitalWrite(CMDled, LOW);
		DoTransmit = DoTransmitOld;
		SideTone = SideToneOld;
		MessageID = 0;
		Record = false;
	}
		
	bool CheckNumeric(uint8_t x)
	{
		if(XChar == 'T') XChar = '0'; // Allow dash for zero
		if((XChar >= '0') && (XChar <= '9')) return true;
		else return false;
	}

// ********  SPEED CHANGE COMMAND FROM PADDLE *****************************
	
	void DoSpeedChange()
	{
		SpeedPot = false; // Don't use speed pot to set speed
		staticPot = analogRead(POT_ADC); // save current for break-out
		speed =  (CMND_SEQ[0] - '0') * 10; // tens digit
		speed = speed + (CMND_SEQ[1] - '0'); // ascii to numeric, then add
		if(speed > 50) speed = 30; // reasonableness check, 50 max
		if(speed < 10) speed = 10;
	}

// *************  HAND KEY MODE *******************************************

// In the hand key mode, if either paddle is closed, then do the key down
// stuff. Else do the key up stuff. If both paddles are closed, exit the
// hand key mode.
// 6/18/2019 - add T/R relay control to hand key

		void HandKey()
		{
			bool keyclosed = false;
			
			while (!((digitalRead(DotContact)==LOW) && (digitalRead(DashContact)==LOW)))
			{
			if((digitalRead(DotContact)==LOW) || (digitalRead(DashContact)==LOW))
			{
				if(controlTR && !TRrelayON)
				{
					digitalWrite(TX_RX, TX_RX_Tstate); // energize T/R relay
					delay(TXOnDelay);
					TRrelayON = true;
				}
				if(!keyclosed)
				{

					digitalWrite(KeyOut, keyDNstate); 
					if(SideTone) tone(spkrpin, pitch); // produce tone
					digitalWrite(LEDpin, HIGH);
					keyclosed = true;
				}
				
			}
			
			else
				
			{
				if(keyclosed) // time to open, but not open yet
				{
					timingTXHold = millis(); // start timing key open period
					digitalWrite(KeyOut, keyUPstate);  // open it
					if(SideTone) noTone(spkrpin);
					digitalWrite(LEDpin, LOW);
					keyclosed = false; // flag that it is open
				}
				if((millis() >= (timingTXHold + (TXHold * 60))) && TRrelayON)
				{
					digitalWrite(TX_RX, TX_RX_Rstate); // drop out the relay
					TRrelayON = false;
				}
			}
			}
		}

// **************** SET SIDETONE PITCH ************************************

// Dash paddle makes pitch higher. Dot makes it lower.
// Both paddles closed exits routine with new pitch
// Added delay and recheck because both paddles was hard to detect
// I originally made my steps about a semitone (36 Hz) at 600 Hz but
// that seems to large in practice, so going lower.

	void PitchSet()
	{
		while (!((digitalRead(DotContact)==LOW) && (digitalRead(DashContact)==LOW)))
		{
		
		if(digitalRead(DashContact)==LOW)
		{
			delay(200);
			if(digitalRead(DotContact)==LOW) goto imdone;
			pitch = pitch + 20;
			if(pitch > 999) pitch = 999; // keep it to 3 digits, for one thing
			test_beep();
		}
		if(digitalRead(DotContact)==LOW)
		{
			delay(200);
			if(digitalRead(DashContact)==LOW) goto imdone;
			pitch = pitch - 20;
			if(pitch < 200) pitch = 200;
			test_beep();
		}
		}
		
		imdone:;
		Serial.print("\nPitch: ");
		Serial.println(pitch, DEC);
	}

	void SayTRState() // announce T/R control is on or off
	{
		Serial.print("\nT/R control is ");
		morseSendString(" = T/R ");
		if(controlTR)
		{
			Serial.println("ON");
			morseSendString("ON = ");
		}
		else
		{
			Serial.println("OFF");
			morseSendString("OFF = ");
		}
	}
	
	void SendInfo() // sends all configuration info in Morse and serial
	{
		RptDlyTimetoStr(); // convert msg rpt delay time to ascii V1.1a
		MsgsToSerial(); // Put all four messages to serial port
		
		SpeedToString(speed_old); // ******** SPEED **********
		delay(700);
		morseSendString(SpeedString);
		Serial.println();
		Serial.println(SpeedString);
		
		if(Any()) goto quit_it;	
		
		morseSendString(" = "); // spacer
		
		if(Any()) goto quit_it;
		
		PitchToString(); // ************* PITCH ***************
		morseSendString(PitchString);
		Serial.println(PitchString);
		if(Any()) goto quit_it;
		
		morseSendString(" = MODE "); // *********** MODE ******
		if(ModeA) send_code('A');
		else send_code('B');
		
		//morseSendString(" = "); // spacer
		Serial.print("MODE ");
		if(ModeA) Serial.println("A");
		else Serial.println("B");
		
		if(Any()) goto quit_it;
		
		morseSendString(" = "); // ***** TX LINE LOGIC ********
		if(Any()) goto quit_it;
		
		if(keyDNstate == LOW)
		{
			morseSendString("TX LO");
			Serial.println("TX logic LOW");
		}
		else
		{
			morseSendString("TX HI");
			Serial.println("TX logic HIGH");		
		}
		
		if(Any()) goto quit_it;	
		
		// morseSendString(" = "); //V1.1a: SayTRState has = in it
		
		if(Any()) goto quit_it;
		
		SayTRState(); // T/R relay control enabled Y/N?
		
		if(Any()) goto quit_it;
		
		if(SpeedPot) // ***** SPEED POT YES OR NO *************
		{
			morseSendString("POT YES");
			Serial.println("SPEED POT: YES");
		}
		else
		{
			morseSendString("POT NO");
			Serial.println("SPEED POT: NO");
		}
		
		if(Any()) goto quit_it;		
		morseSendString(" = ");
		if(Any()) goto quit_it;	


		if(MsgRptTime) // ***** MESSAGE REPEAT YES/NO AND TIME *************
		{
			morseSendString("REPEAT DLY ");
			morseSendString(MsgRptTimeStr);
			Serial.print("MESSAGE REPEAT DELAY: ");
			Serial.println(MsgRptTimeStr);
		}
		else
		{
			morseSendString("MSG RPT NO");
			Serial.println("MESSAGE REPEAT: NO");
		}
		
		if(Any()) goto quit_it;		
		morseSendString(" = ");
		if(Any()) goto quit_it;	

		sayRev(); // Send revision in Morse
		
		quit_it:;
		
		morseSendString(" <"); // < sends as [AR]
		
	}
	
	void MsgsToSerial()
	{
		uint8_t countr = 0;
		uint8_t bytenow = ' ';		

		Serial.println("\nMessage A:");
		
		countr = 0;
		while(bytenow)
		{
			EEPROM.get(E_MEM_A+countr, bytenow);
			Serial.write(bytenow);
			countr++;
			if(countr > 75) break;
		}
		Serial.println("\n"); // two blanklines
		
		Serial.println("\nMessage B:");
		countr = 0;
		bytenow = ' ';
		while(bytenow)
		{
			EEPROM.get(E_MEM_B + countr, bytenow);
			Serial.write(bytenow);
			countr++;
			if(countr > 75) break;
		}
		Serial.println("\n"); // two blanklines		
		Serial.println("\nMessage C:");
		countr = 0;
		bytenow = ' ';
		while(bytenow)
		{
			EEPROM.get(E_MEM_C + countr, bytenow);
			Serial.write(bytenow);
			countr++;
			if(countr > 75) break;
		}
		
		Serial.println("\n"); // two blanklines
		
		Serial.println("\nMessage D:");
		countr = 0;
		bytenow = ' ';
		while(bytenow)
		{
			EEPROM.get(E_MEM_D + countr, bytenow);
			Serial.write(bytenow);
			countr++;
			if(countr > 75) break;
		}
		Serial.println("\n"); // two blanklines		
	}	
// *********************  END of INFO REPORT ******************************



// ******************* RECORDING A MESSAGE MEMORY *************************

	void Recording()
	{
		//Serial.println("REC");
		if(XChar == 0) // 0 created by [QT] means cancel message recording
		{
			//Serial.println("Failed on XChar");
			MsgBuffer[MessagePtr] = 0; // NULL flags end of message
			MessageID = 0;
			ExitCMND();
			rasp(); // negative sound
			goto cancelmsg;
		}
		
		//if(AnyButton) XChar = 27;
		if(XChar == ' ') short_sound();
		if(XChar != 27) // [EOM] or any button ends recording
		{
			// It's hard to not start with a leading space, so I was going to
			// suppress it, but I found it sounds better with the space.
			//if(!(XChar == ' ' && !MessagePtr)) // Don't record first space
			//{
			MsgBuffer[MessagePtr] = XChar;
			MessagePtr++;
			//}
		}

		else
		{
			//Serial.print("fINISHED w/ PTR = "); // ******** TEMP **********
			//Serial.println(MessagePtr); // ******** TEMP **********
			good_sound();
			MsgBuffer[MessagePtr] = 0; // NULL flags end of message
			BurnMsg();
			MessageID = 0;
			ExitCMND();	
		}
		cancelmsg:;
	}
	
	void BurnMsg()
	
	{
	/*	
		misc_count = 0; // ******** TEMP **********
		while(misc_count < 10) // ******** TEMP **********
		{
			Serial.write(MsgBuffer[misc_count]);
			misc_count++; // ******** TEMP **********
			
		} // ******** TEMP **********
		Serial.print("1st char: "); // ******** TEMP **********
		Serial.println(MsgBuffer[0]);
		
		Serial.print("Message to Burn:"); // ******** TEMP **********
		*/
		
		MessagePtr = 0;
		while((MsgBuffer[MessagePtr] != 0) && (MessagePtr < 76))
		{
			//Serial.write(MsgBuffer[MessagePtr]); // ******** TEMP **********
			EEPROM.put((MsgAdr + MessagePtr), MsgBuffer[MessagePtr]);
			MessagePtr++;
		}
		EEPROM.put((MsgAdr + MessagePtr), 0);
		//Serial.print("\nBurned ");
		//Serial.println(MessagePtr, DEC);
		//Serial.print("Last Address: ");
		//Serial.println((MsgAdr + MessagePtr), DEC);
	}

/*		
	void MsgToSerial()
	{
		uint8_t icount;
		uint8_t z = ' ';
		MessagePtr = 0;
		for(icount = 0; icount < 50; icount++)
		{
			EEPROM.get((E_MEM_A + MessagePtr), z);
			if(z == 0) break;			
			Serial.write(z);
			MessagePtr++;
			
		}
	}
	*/
// ********* PARSE & STORE MESSAGE ENTERED FROM SERIAL PORT ***************


	void MsgFromSerial()
	{
		//while(Serial.read() != -1); // flush by read until empty
		Serial.println("\nReady to record message.\n1st char is message ID");		
		morseSendString(" OK SERIAL");

		
		while(!Serial.available());
		//{
			SerialMsg = Serial.readString();
		//}
		
		/*
		if(SerialMsg.length() == 0)
		{
			ExitCMND();
			goto NoMsg;
		}
		*/
		SerialMsg.toUpperCase();
		Serial.println("\nOK,your message is: ");
		Serial.println(SerialMsg);
		Serial.print("Message length: ");
		Serial.println(SerialMsg.length());
		
		MessagePtr = 0;
		uint8_t ID;
		ID = SerialMsg[MessagePtr]; // 1st char is MSG ID
		
				if((ID < 'A') || (ID > 'D'))
			{
				ExitCMND();
				//Serial.println("\nDidn't pass A-D");
				goto NoMsg;
			}
		else
			{
				MessageID = ID; // Here known to be recording msg
				if(ID == 'A') MsgAdr = E_MEM_A;
				if(ID == 'B') MsgAdr = E_MEM_B;
				if(ID == 'C') MsgAdr = E_MEM_C;
				if(ID == 'D') MsgAdr = E_MEM_D;
			}
	
	// Now I get chars from the string and send one at a time to Recording(),
	// parsing so that 10, 13 or 0 are translated to 27 which causes normal
	// termination. The Recording() routine then burns the EEPROM. Also,
	// Recording() iterates MessagePtr. When it sets MessageID to 0, I know
	// it is finished.
	// Note offset of 1 because buffer 1st char was message ID
	
		MessagePtr = 0;
		
		while(MessageID)
		{
			XChar = SerialMsg[MessagePtr + 1];
			
			if((XChar == 10) || (XChar == 13) || !XChar)
			{
				XChar = 27; // Newline, LF, CR or 0 all mean end message
			}
			Recording();
			/*
			if(MessagePtr == 3)
			{
				Serial.print("3rd char is ");
				Serial.write(XChar);
				Serial.println();
			}
			*/
		}
		
		NoMsg:;
	}
	
	
// ********** PLAYING (SENDING) A STORED MESSAGE **************************

/*
	void PlayMsg(uint8_t ID)
	{
		uint8_t msgchar = 1;
		uint8_t msgcountchar = 0;
		
		if(ID == 'A') MsgAdr = E_MEM_A;
		if(ID == 'B') MsgAdr = E_MEM_B;
		if(ID == 'C') MsgAdr = E_MEM_C;
		if(ID == 'D') MsgAdr = E_MEM_D;
		
		while(msgchar)
		{	
			EEPROM.get((MsgAdr + countchar), msgchar);
			countchar++;
			if(msgchar) send_code(msgchar);
			
			if(Any()) // Paddle or other action stops message
			{
				delay(500); // allow getting off paddle, avoid extra junk sent
				break; 
			}
		}
		ExitCMND();
	}
	
*/

// *************  GET A CHARACTER FROM MESSAGE MEMORY *********************

// This routine gets the character and puts into XChar. If it is 0, it
// stops the sender with the MsgActive flag. If it's an ordinary char,
// it sends it as Morse. If it's part of a command sequence, it returns 
// it for processing.

// 7/1/2009 Change logic so any action (PB, paddle) terminates message
// sending.


	void MsgGetChar()
	{
		//Serial.println("Sending MSG"); // ******* TEMP ********
		
			
		//if(msgcountchar == 0) delay(100); // V1.1 debounce msg button
		// V1.1 adding msgcountchar below is intended to skip the user action
		// check on the 1st char, because switch bounce was causing 
		// unwanted cancellations
		
		while(XChar)
		{
			if(Any() && msgcountchar) // any user action stops message sending
			{
				XChar = 0;
				MsgActive = false; // stop sender if 0 was found
				MsgAdrSave = 0; // flag no return address
				delay(250); //V1.1 prevent contact bounce restarting msg
			}			
				
			

			EEPROM.get(MsgAdr + msgcountchar, XChar);
			msgcountchar++;
		
			// if a message is ending and it was called from another
			// message, get address info from that one and pick up
			// there.
			
		
			if(!XChar)
			{
				if(MsgAdrSave && (MsgAdr != MsgAdrSave)) 
					// non-zero means we have return address
				{
					MsgAdr = MsgAdrSave;
					msgcountchar = msgcountcharSave;
					// I can't go to send_code() with XChar = 0, so I'll
					// get next character. (The morse[] table starts at
					// 44 so 0 would index an unknown morse pattern)
					
					EEPROM.get(MsgAdr + msgcountchar, XChar);
					msgcountchar++;		
					
					//Serial.print("Base: ");
					//Serial.println(MsgAdr, DEC);
					//Serial.print("Count: ");
					//Serial.print(msgcountchar, DEC);
				}
				else
				{
					// V1.1a: time to stop message, but first see if repeat
					// is active and if this is message A, then start over
					
				if(MsgRptTime && (MsgAdr == E_MEM_A))
				{
					// Start message over if no user cancel action ...
					
					if(DelayAndCheck(MsgRptTime * 100))
					{
					msgcountchar = 0;	
					EEPROM.get(MsgAdr + msgcountchar, XChar); // get 1st char
					msgcountchar++;

// Logic got a little crazy here with all the goto and else statements

					goto sendthechar;
					}
					else goto dostopit;
				}
				else
					dostopit:;
					{
					while(Any()); // hold until user releases paddle/button
					delay(250); // debounce
					MsgActive = false; // stop sender if 0 was found
					MsgAdrSave = 0; // flag no return address
					}
				}
			}
			
			sendthechar:;
			
			if((XChar != '*') && !CMNDMode && MsgActive ) // not cmnd related, just send
			{
				send_code(XChar);
				Serial.write(XChar);
			}
			
		// Below, XChar is not 0 and sending is still active, but I exit
		// so main loop can process command related character
		
			else goto takecmdhome;
		}
		takecmdhome:;
	}

// ******** SET UP FOR MESSAGE START **********************

	void MsgStart(uint8_t ID)
	{
		// if starting message and a message is already running, need
		// to save current address & pointer to continue from:
		
		if(MsgActive)
		{
			MsgAdrSave = MsgAdr;
			msgcountcharSave = msgcountchar;
		}
		
		CMNDMode = false; // While in message send mode, I will not be in
		CMNDChar = 'Z'; // CMND mode so I can have embedded commands
		digitalWrite(CMDled, LOW);
		if(ID == 'A') MsgAdr = E_MEM_A;
		if(ID == 'B') MsgAdr = E_MEM_B;
		if(ID == 'C') MsgAdr = E_MEM_C;
		if(ID == 'D') MsgAdr = E_MEM_D;
		
		MsgActive = true; // tell mainline we are sending a message
		msgcountchar = 0; //point to start
	}
		
		
		
	void SpeedToString(uint8_t oldspeed)
	{
		SpeedString[0] = oldspeed/10 + '0'; // tens digit
		SpeedString[1] = oldspeed%10 + '0'; // ones digit
		
	}
	
	void RptDlyTimetoStr()
	{
		MsgRptTimeStr[0] = MsgRptTime/10 + '0'; // tens
		MsgRptTimeStr[2] = MsgRptTime%10 + '0'; // 1s, note [1] is decimal pt
	}
		
	
	void PitchToString()
	{
		PitchString[0] = pitch/100 + '0'; // hundreds
		PitchString[1] = (pitch/10)%10 + '0'; // tens
		PitchString[2] = pitch%10 + '0'; // ones
	}
// **************  READ SPEED POT AND CALCULATE SPEED *********************
// This got a little cumbersome as my integer math mixing 8 and 16 bits
// got flaky as the ADC value approached maximum. My final trick was to just
// go with floating point and convert.

	void readSpeedPot()
	{
		float speed_x;
		// speed = SPD_LIMIT_LO; 
		speed_x = SPD_LIMIT_LO + (SPD_LIMIT_HI - SPD_LIMIT_LO) * (float) analogRead(POT_ADC)/1023.0;
		speed = (uint8_t)  (speed_x + 0.5); // casting just truncates
		morseSetSpeed(); // calculates dot/dash/space times in ms
		
		// below checking for any jitter. Didn't see any ...
		//Serial.println(speed, DEC);
		//delay(500);
		
	}


	
// ************* RE-ENABLE SPEED POT IF CHANGE > THRESHOLD ****************

	// If the speed pot has been disabled by manual speed entry, this 
	// function checks to see if the pot has been moved >= 3 WPM and
	// if so, re-enables it.
	
	void freeSpeedPot()
	{
		if(abs(staticPot - analogRead(POT_ADC)) > 87) SpeedPot = true;
		
	}
	
// **** CHECK MY EE_CODE FLAG TO SEE IF EE HAS BEEN WRITTEN **************

	bool EE_written()
	{		
		EEPROM.get(E_EE_CODE, ee_code); 
		if(ee_code != EE_CODE)return false;
		else return true;
	}

// ****************  SAVE SETUP TO EEPROM *********************************

	void EESave()
	{
		EEPROM.put(E_speed, speed);
		EEPROM.put(E_pitch, pitch);
		EEPROM.put(E_keyDNstate, keyDNstate);
		EEPROM.put(E_SideTone, (byte) SideTone);
		EEPROM.put(E_ModeA, (byte) ModeA);
		EEPROM.put(E_SpeedPot, (byte) SpeedPot);
		EEPROM.put(E_EE_CODE, EE_CODE);
		EEPROM.put(E_TR_RELAY, (byte) controlTR);
		EEPROM.put(E_MsgRptTime, (byte) MsgRptTime); // V1.1a
		delay(250); // V1.1
		SayOK(); // Morse OK to tell user EEPROM write done
	}
	
// ****************** RECALL SETUP PARAMS FROM EEPROM *********************

	void EERecall()
	{
		EEPROM.get(E_speed, speed);
		EEPROM.get(E_pitch, pitch);
		EEPROM.get(E_keyDNstate, keyDNstate);
		EEPROM.get(E_SideTone, SideTone);
		EEPROM.get(E_ModeA, ModeA);
		EEPROM.get(E_SpeedPot, SpeedPot);
		EEPROM.get(E_EE_CODE, ee_code);
		EEPROM.get(E_TR_RELAY, controlTR);
		EEPROM.get(E_MsgRptTime, MsgRptTime); // V1.1a
		morseSetSpeed();
		keyUPstate = !keyDNstate;
	}
	
// V1.1 Send 'OK' via sidetone

// Routine sets speed to 25 and pitch to 550 Hz and restores old
// values on exit. 

	void SayOK()
		{
				unsigned int old_pitch;
				old_pitch = pitch;
				pitch = 550;
				speed_old = speed;
				speed = 25;
				morseSetSpeed();
				SideToneOld = SideTone;
				SideTone = true; // do send sidetone
				DoTransmitOld = DoTransmit;
				DoTransmit = false; // don't send on the air
				send_code('O');
				send_code('K');
				pitch = old_pitch;
				speed = speed_old;
				morseSetSpeed();
				DoTransmit = DoTransmitOld; // restore previous
				battTimer = millis();
				SideTone = SideToneOld;
				
			}

// *************** ANY - DETECT ANY USER ACTION ***************************

		bool Any()
		{
			bool rvalue = false;
			if(!digitalRead(DotContact)) rvalue = true;
			if(!digitalRead(DashContact)) rvalue = true;
			if(!digitalRead(PB1)) rvalue = true;
			if(!digitalRead(PB2)) rvalue = true;
			
			if(rvalue) // V1.1 - stay until action is released:
				while( !digitalRead(DotContact) || !digitalRead(DashContact)
						|| !digitalRead(PB1) || !digitalRead(PB2)); 
				
			return rvalue;
		}

 // *** DelayAndCheck will delay the number of ms in the argument unless
 // any user action occurs. Then it terminates and returns false. If it goes
 // to completion, it returns true. *** V1.1a
 

	bool DelayAndCheck(uint16_t mdelay)
	{
		bool hometruth = true;
		unsigned long targetTime;
		targetTime = millis() + mdelay;
		
		while(millis() < targetTime && hometruth == true)
		{
			if(Any()) hometruth = false;
		}
		
		return hometruth;
	}
		
		
	
	bool AnyButton() // Returns true if any button pressed, else false
	{
		bool rvalue = false;
		if(!digitalRead(PB1)) rvalue = true;
		if(!digitalRead(PB2)) rvalue = true;
		return rvalue;
	}
	
// **********************  LOCK OUT ON STUCK PADDLE ***********************	
	
	void LockOut()
	{
		
		// This routine got a little complicated as I was chasing a bug, but
		// it's still good.
		
		digitalWrite(KeyOut, keyUPstate); // open keyed line
		noTone(spkrpin); // stop tone
		while(digitalRead(DotContact)==LOW || digitalRead(DashContact)==LOW) // stay while either or both closed
		{
			tone(spkrpin, 600); // warble tone to alert user to lockout
			delay(250);
			tone(spkrpin, 566);
			delay(250);
			noTone(spkrpin);
			delay(500);
			timing_start = millis(); // get back into range of unit8_t
		}
		
		DeadMan = 255;
		DoLockout = false;
	}
// ************************************************************************

// **********   ADDING THE CONTENTS OF Morse.ino here *************



/*


In Morse, the length of a dit or space in seconds = 1.2/WPM, in ms = 1200/WPM

Timings are (in dit-lengths):

dit:      1
dah:      3
inter-sp  1    (spaces between elements of a character)
char sp   3    (between letters)
word sp   7    (between words)


Arduino has a function NewTone(pin, freq, duration).  Duration is optional and is in ms.
If duration is not used, call noNewTone(pin) to shut it off.  Output is a 50% square 
wave. Note: NewTone is not a built in library, I had to download the zip, then use
sketch>Include Library>Add ZIP Library to bring it in.

*** I couldn't get it to work with the duration argument, so I switched to turning it
    on, timing the duration separately, then turning it off. ***

***  I'm using a 12 mm sounder I got from Digikey CEM-1201(50).  It's plenty loud.  I have 
     180 ohm resistor in series with it and connect it between Pin 7 and ground.  In its 
	  data sheet it has a resonance over 2,000 Hz and another one around 600 Hz, so I'm using
	  600 Hz.
***	V1.1 - I keep having trouble with NewTone disappearing, so I'm going to
    just tone()

*/


// Morse characters are stored like this:  The high bit represents the first
// element of a character.  0 is a dot and 1 is a dash.  The final 1 flags
// the end of this character and is not sent.  Therefore 'B' being _... is 
// stored as 0b10001000
// Sending happens by checking the high bit and sending dot or dash as
// appropriate, then left shifting the data by one bit.  Before sending though,
// if the data = 0b10000000, then stop.  Sending of this character is finished.

// There are some gaps in ASCII codes that have characters I don't need and
// so waste some RAM, but not enough to do special remapping.
// From 44 through 90 are 47 bytes and I figure 41 are moderately necessary.

// Notice below how it's possible to break lines and even have comments
// interspersed

byte morse[47] = {0b11001110, 0b10000110, 0b01010110,// ','  '-'  '.' 44-46
					0b10010100, 0b11111100, 0b01111100,// /, 0, 1  47-49
					0b00111100, 0b00011100, 0b00001100, // 2, 3, 4 50-52
					0b00000100, 0b10000100, 0b11000100, // 5, 6, 7 53-55
					0b11100100, 0b11110100, 0b10110100, // 8, 9, : (KN) 56-58
					0b10101010, 0b01010100, 0b10001100, // ; (KR), < (AR), = (BT) 59-61
					0b00010110, 0b00110010, 0b01101010, // > (SK), ?, @ (AC) 62-64
					0b01100000, 0b10001000, 0b10101000, // ABC starts at 65 DEC
					0b10010000, 0b01000000, 0b00101000, // DEF
					0b11010000, 0b00001000, 0b00100000, // GHI
					0b01111000, 0b10110000, 0b01001000, // JKL
					0b11100000, 0b10100000, 0b11110000, // MNO
					0b01101000, 0b11011000, 0b01010000, // PQR
					0b00010000, 0b11000000, 0b00110000, // STU
					0b00011000, 0b01110000, 0b10011000, // VWX
					0b10111000, 0b11001000};             // YZ ends at 90 DEC


void dit()
	{
		if(DoTransmit)
		{
			digitalWrite(KeyOut, keyDNstate); // close keyed line
		}
		if(SideTone) tone(spkrpin, pitch);
		space();
		if(SideTone) noTone(spkrpin);
		digitalWrite(KeyOut, keyUPstate); // open keyed line
		space();
	}

void dah()
	{
		if(DoTransmit)
		{
			digitalWrite(KeyOut, keyDNstate); // close keyed line
		}		
		if(SideTone) tone(spkrpin, pitch);
		space();
		space();
		space();
		if(SideTone) noTone(spkrpin);
		digitalWrite(KeyOut, keyUPstate); // open keyed line
		space();
	}

void space()
	{
	   delay(ditlen); // delays ditlen milliseconds
	}

void char_space()
	{
		delay(2*ditlen);
	}
void word_space()
{
	  delay(4*ditlen);
}

// ******  SEND CHARACTER STRING IN MORSE *********************************


void send_code(int pointtocode)
{
	byte pattern;
	if (pointtocode == ' ')
	{
	   word_space();
	}
	else
	{
	pointtocode -= 44; // shift ASCII pointer to begin at 0 for comma
	pattern = morse[pointtocode];
	
	while (pattern != 128)
	{
		if (pattern & 0b10000000)
		{
			dah();
		}
		else
		{
		   dit();
		} 
		
		pattern <<= 1;
		
	}
	char_space();
	}
}		



void morseSetSpeed()
{
    ditlen = 1200/speed;
    dahlen = ditlen * 3;  
	//halfspace = ditlen/2;
	
	// halfspace = 2; // Change from half space to fixed 2 ms
	// As of 6/10/2019, I'm trying 'halfspace' equal to about 75% of
	// a space, mimicking what the K3, KX3 and FT-991A do.
	
	halfspace = ditlen/2 + ditlen/4;
	
	wordspace = 6 * (uint16_t) ditlen;
}

void sayRev()
{
	char morsechar;
	
	delay(100);
	char introduction[] = REV_TEXT; 
	for(int x=0; introduction[x]; x++)
	{
		send_code(introduction[x]);
	}
	Serial.println(REV_TEXT);
}

void morseSendString(char mstring[])

    {
      for(int x=0; mstring[x]; x++)
        {
            send_code(mstring[x]);
        }
        
    }

// trying for a slightly negative sound for error, or cancel:

	void rasp()
	{
		uint8_t icount = 70;
		/*
		short_sound();
		delay(50);
		short_sound();
		delay(50);
		short_sound();
		delay(50);
		short_sound();
		delay(50);
		*/
		while(icount)
		{
			digitalWrite(spkrpin, HIGH);
			delay(1);
			digitalWrite(spkrpin, LOW);
			delay(1);

			digitalWrite(spkrpin, HIGH);
			delay(2);
			digitalWrite(spkrpin, LOW);
			delay(2);			
			icount--;			
		}
			
		
	}
	
    void short_sound()
    {
			delay(50);
            tone(spkrpin, pitch); // give user short beep feedback
            delay(25);
            noTone(spkrpin);
    }
	
	void test_beep()
	{
		tone(spkrpin,pitch);
		delay(250);
		noTone(spkrpin);
		delay(200);
	}
	
	// Play a descending tone to indicate a bad outcome
	
	void bad_sound()
	{
		delay(150);
		tone(spkrpin, pitch);
		delay(150);
		tone(spkrpin, (pitch - 35));
		delay(150);
		noTone(spkrpin);
	}	
	
	// ascending tone indicates good outcome
	
	void good_sound()
	{
		delay(100);
		tone(spkrpin, pitch -15);
		delay(100);
		tone(spkrpin, (pitch + 15));
		delay(100);
		noTone(spkrpin);
	}	

	void SingleBeep() // A beep of 40 WPM speed
	{
		delay(20);
		tone(spkrpin, pitch);
		delay(30);
		noTone(spkrpin);
		delay(20);
	}
	
	void DoubleBeep() // Two dits at 40 WPM speed (30 ms/dit)
	{
		delay(20);
		tone(spkrpin, pitch);
		delay(30);
		noTone(spkrpin);
		delay(30);
		tone(spkrpin, pitch);
		delay(30);
		noTone(spkrpin);
		delay(20);
	}
		
		
		