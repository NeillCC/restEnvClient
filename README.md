# restEnvClient
Simple program to run a DHT11/22 Sensor on a esp32 Devboard.
On start, a WiFi network is created and protected by "password". After joining you can configure: 
- WiFi SSID
- WiFi password
- HomeAssistant server
- HomeAssistant port
- HomeAssistant access token
- HomeAssistant update frequency
- device hostname
- friendly name

Home assistant is updated with the Celsius, Fahrenehit, and Humidity readings via HTTP POST.
