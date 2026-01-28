/*
 * html_page.h
 *
 *  Created on: Mar 25, 2025
 *      Author: Daruin Solano
 */

#ifndef NET_INC_HTML_PAGE_H_
#define NET_INC_HTML_PAGE_H_


const char http_header_reponse[] __attribute__((section(".rodata"))) =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n";

const char CREDENTIALS_UPDATE_HTML_PAGE[] __attribute__((section(".rodata"))) =
		"<!DOCTYPE html>\r\n"
		"<html>\r\n"
		"<head>\r\n"
		"<title>Wi-Fi Configuration</title>\r\n"
		"</head>\r\n"
		"<body>\r\n"
		"<h2>Update Wi-Fi Credentials</h2>\r\n"
		"<form action=\"/update_wifi\" method=\"post\">\r\n"
		"<label for=\"ssid\">SSID:</label>\r\n"
		"<input type=\"text\" id=\"ssid\" name=\"ssid\" required><br><br>\r\n"
		"<label for=\"password\">Password:</label>\r\n"
		"<input type=\"password\" id=\"password\" name=\"password\" required><br><br>\r\n"
        "<input type=\"submit\" value=\"Update\">\r\n"
		"</form>\r\n"
		"</body>\r\n"
		"</html>\r\n"
;


#endif /* NET_INC_HTML_PAGE_H_ */
