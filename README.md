# ESP-IDF web_server_uploader app (example)

Uploading OTA image file and any html files.

## Note

`/index.html`, `/favicon.ico`, `/style.css` and `scripts.js` can be overridden by uploading files with same names.

## Usage

* Open the project configuration menu (`idf.py menuconfig`) go to `Example Configuration` ->
    1. WIFI SSID: WIFI network to which your PC is also connected to.
    2. WIFI Password: WIFI password

* In order to test the file server demo :
    1. compile and burn the firmware `idf.py -p PORT flash`
    2. run `idf.py -p PORT monitor` and note down the IP assigned to your ESP module. The default port is 80
    3. test the example interactively on a web browser (assuming IP is 192.168.100.40):
        1. open path `http://192.168.100.40` or `http://192.168.100.40/index.html` to see an HTML web page with upload menu
        2. use the file upload form on the webpage to select and upload a file to the server
        3. uploading a firmware file or html files (\*.html, \*.css, \*.js or other)
	
	