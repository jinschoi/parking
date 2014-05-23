Ultrasonic Parking Aid
=======

Parking aid using the SR-04 ultrasonic distance sensor to indicate distance via LEDs. Low-power, AA battery powered, 3D printable enclosure suggestion available.

Green LED indicates go until yellow LED gets triggered at a set distance. Yellow LED blinks with a rate proportional to distance until red LED distance is crossed. LED blinking rate is implemented using TIMER1.

Low power mode is entered when no forward motion events have been detected for 5 seconds. Motion away from sensor is ignored so as not to trigger when backing away.

Libraries Required
-----

_Sleepy_ library from [jeelib](http://jeelabs.net/projects/jeelib/wiki) is used to reduce power usage when idling.

[NewPing](https://code.google.com/p/arduino-new-ping/) library is required for interfacing with the ultrasonic sensor.

Full description at [hackaday.io](http://hackaday.io/project/1218-Ultrasonic-Parking-Aid-without-Arduino).
