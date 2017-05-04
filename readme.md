# iotsaDoorOpener - open a door with RFID or a web browser

Operates a solenoid to open a door. On web access, or when an RFID tag (such as a keychain fob or a mifare contactless transport card) is presented. RFID cards are programmable (over the net, or using a special "learn" card). A web request can be sent to a programmable URL when a card is presented.

The RFID reader functionality and solenoid functionality are contained in individual iotsa extension modules and can be reused, independently of each other.

Home page is <https://github.com/cwi-dis/iotsaDoorOpener>.
This software is licensed under the [MIT license](LICENSE.txt) by the   CWI DIS group, <http://www.dis.cwi.nl>.

## Software requirements

* Arduino IDE, v1.6 or later.
* The iotsa framework, download from <https://github.com/cwi-dis/iotsa>.
* The mfrc522 RFID library, originally by Miguel Balboa, <https://github.com/miguelbalboa/rfid>.

## Hardware requirements

* a iotsa board (or, with some work, another esp8266-based board).
* An MFRC522 RFID reader (search for RC522).
* A electromagnetic door opener, usually a solenoid of some kind.
* A 2N7000 or BS170 MOSFET.
* A NeoPixel, if you want visual feedback.
* Possibly a small relay, like SIP-1A05.

## Hardware construction

The main challenge in construction is catering for the power consumption of the solenoid. The MOSFET is going to be driven with only 3 volt, meaning it will only let about 50-100 mA of current flow (not the 1A max for which the 2N7000 and BS170 are rated). Whether this is enough for your solenoid you have to test, if it is not you need to add a small relay: the MOSFET will trigger the relay easily, and the relay will trigger the solenoid.

Instructions for constructing the hardware are in the _extras_ subfolder:

* [doorOpenerBreadboard.pdf](extras/doorOpenerBreadboard.pdf) is the schematic for use on a breadboard, without a relay. The [Fritzing](http://fritzing.org/home/) project is also available as [doorOpenerBreadboard.fzz](extras/doorOpenerBreadboard.fzz).
* [doorOpenerIotsa.pdf](extras/doorOpenerIotsa.pdf) shows how to put this version together on the iotsa board itself. Fritzing file is there too.
* [doorOpenerIotsaRelay.pdf](extras/DisplayServer-stripboard.pdf) shows how to put the bits together on a stripboard. Again, also available as Fritzing file.

About enclosures: it may be a good idea to mount only the RFID reader on the outside of the door (and the iotsa board and the connection to the solenoid on the inside). If everything is on the inside the RFID reader may not have enough range to get through the door (or frame). If everything is on the outside it is too easy to open the case and operate the solenoid by shorting power to the solenoid.

If you want to mount in two cases: a 3D-printable model for the RFID enclosure can be found at <http://a360.co/2oYSEOF> and an enclosure for the iotsa board can be found in the _extras_ folder of the [Iotsa library](https://github.com/cwi-dis/iotsa).

## Building the software

If you have not changed anything in the design there is nothgin to configure. If you have used different GPIO pins look at the _iotsaDoorOpener_ main program and the _iotsaRFID.h_ and _iotsaSolenoid.h_ include files.

Compile, and flash either using an FTDI or over-the-air.

## Operation

### Initial installation

The first time the board boots it creates a Wifi network with a name similar to _config-iotsa1234_.  Connect a device to that network and visit <http://192.168.4.1>. Configure your device name (we will use _door_ as an example), WiFi name and password, and after reboot the iotsa board should connect to your network and be visible as <http://door.local>.

Visit <http://door.local/door> and use the _Open_ button to operate the solenoid for 2 seconds. If it does not work you may need to add the relay, see _Hardware construction_.

Visit <http://door.local/rfid> to configure RFID cards. The unique ID of the last card presented to the reader is shown here. 

You can add and remove cards that will open the door here too. 

You can also set a "_Master Addition_" and "_Master Removal_" card here. These card will _not_ open the door, but in stead allow you to add other cards (or remove them) without going through the web interface. Present _Master Addition_ and then a new card within a couple of seconds and that new card will be added to the list of known cards.

### Normal operation

The LED will flash white once every 5 seconds or so to show that the door opener is working. 

When you present a known RFID card or tag to the reader the LED will light up green for 2 seconds and the solenoid will operate. 

When you present an unknown card the LED will light up red for two seconds.

When you present the _Master Addition_ card the LED will quickly flash green. You have 2 seconds to present the new card. The LED will go solid green, the door will open, and the new card is now a known card.

When you present the _Master Deletion_ card the LED will quickly flash red. You have 2 seconds to present a known card. The LED will go solid red, and the card is no longer a known card and cannot be used to open the door any more.