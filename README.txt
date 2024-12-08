1A client:
client www.ee.nuk.edu.tw 80
GET / HTTP/1.0

1B server:
server 8080
http://localhost:8080/

2A webget:
webget http://gaia.cs.umass.edu/wireshark-labs/HTTP-wireshark-file1.html

2B webserv:
webserv 8080
http://localhost:8080/mytest.htm

3A proxy:
proxy 8080
http://localhost:8080
http://gaia.cs.umass.edu/wireshark-labs/HTTP-wireshark-file1.html