#include <Arduino_MKRENV.h>

#define HOMEKIT_LOWER_LIMIT 0.0001

 float temperature = 0;
 float humidity = 0;
 float pressure = 0;
 float illuminance = 0;

/*
* inits the MKR ENV shield
*/
void sensors_init()
{
    if (!ENV.begin())
    {
        Serial.println("ERROR: Failed to initialize MKR ENV shield");
        while (1)
            ;
    }
}

/*
* reads the sensors values
*/
void sensors_loop()
{
    // read all the sensor values
    temperature = ENV.readTemperature();
    humidity = ENV.readHumidity();
    pressure = ENV.readPressure();
    illuminance = ENV.readIlluminance();

    // let the other loops run
    yield();
}

/*
* returns current temperature
*/
float sensors_get_temperature(){
    return temperature;
}

/*
* returns current hmuidity
*/
float sensors_get_humidity(){
    return humidity;
}

/*
* returns current pressure
*/
float sensors_get_pressure(){
    return pressure;
}

/*
* returns current illuminance
*/
float sensors_get_illuminance(){
    if (illuminance < HOMEKIT_LOWER_LIMIT)
    {
        illuminance = HOMEKIT_LOWER_LIMIT;
    }
    return illuminance;
}