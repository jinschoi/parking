Ultrasonic Parking Aid
=======

Parking aid using the SR-04 ultrasonic distance sensor to indicate distance via LEDs. Low-power, AA battery powered, 3D printable enclosure suggestion available.

Green LED indicates go until yellow LED gets triggered at a set distance. Yellow LED blinks with a rate proportional to distance until red LED distance is crossed. LED blinking rate is implemented using TIMER1.

Libraries Required
-----

_Sleepy_ library from [http://jeelabs.net/projects/jeelib/wiki](jeelib) is used to reduce power usage when idling.

[https://code.google.com/p/arduino-new-ping/](NewPing) library is required for interfacing with the ultrasonic sensor.

Full description at [http://hackaday.io/project/1218-Ultrasonic-Parking-Aid-without-Arduino](hackaday.io).
