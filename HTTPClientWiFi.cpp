/*
 * HTTPClient
 * HTTPClient.c
 *
 *  http://interactive-matter.org/
 *
 *  This file is part of HTTPClient.
 *
 *  HTTPClient is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  HTTPClient is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with HTTPClient.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Created on: 18.09.2010
 *      Author: marcus
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <HardwareSerial.h>
#include "HTTPClientWiFi.h"

//a struct to store the uriEncoder & the handle to the http client
typedef struct
{
  HTTPClientWiFi* client;
  uint8_t encode;
#define URI_ENCODE _BV(0)
#define URI_ENCODE_RESERVED _BV(1)
} http_stream_udata;

//we need this probably more than once o it is defined here
#define URI_ALLOWED(byte) ((byte>='A' && byte<='Z') || (byte>='a' && byte<='z') || (byte>='0' && byte<='9') || byte == '-' || byte == '_' || byte == '.' || byte == '~')
#define URI_RESERVED(byte) (byte == '!' || byte == '*' || byte == '\'' || byte == '(' || byte == ')' || byte == ';' || byte == ':' || byte == '&' || byte == '=' || byte == '+' || byte == '$' || byte == ',' || byte == '/' || byte == '?' || byte == '#' || byte == '[' || byte == ']')

HTTPClientWiFi::HTTPClientWiFi(char* host, uint8_t* ip) :
    WiFiClient(), hostName(host), debugCommunication(0), ip(ip), port(80)
{
  //nothing else to do
}

HTTPClientWiFi::HTTPClientWiFi(char* host, uint8_t* ip, uint16_t port) :
    WiFiClient(), hostName(host), debugCommunication(0), ip(ip), port(port)
{
  //nothing else to do
}

FILE*
HTTPClientWiFi::getURI(char* uri)
{
  return getURI(uri, NULL, NULL);
}

FILE*
HTTPClientWiFi::getURI(char* uri, http_client_parameter parameters[])
{
  return getURI(uri, parameters, NULL);
}

FILE*
HTTPClientWiFi::getURI(char* uri, http_client_parameter parameters[],
    http_client_parameter headers[])
{
  FILE* result = openClientFile();
  if (result == NULL)
    {
      return NULL;
    }
  //the request and the default headers
  sendUriAndHeaders(result, this->hostName, PSTR("GET"), uri, parameters,
      headers);
  //ok header finished
  fprintf_P(result, PSTR("\n"));
  skipHeader(result);

  return result;
}

FILE*
HTTPClientWiFi::postURI(char* uri, char* data)
{
  return postURI(uri, NULL, data, NULL);
}
FILE*
HTTPClientWiFi::postURI(char* uri, http_client_parameter parameters[], char* data)
{
  return postURI(uri, parameters, data, NULL);
}

FILE*
HTTPClientWiFi::postURI(char* uri, http_client_parameter parameters[], char* data,
    http_client_parameter headers[])
{
  FILE* result = openClientFile();
  if (result == NULL)
    {
      return NULL;
    }
  sendUriAndHeaders(result, this->hostName, PSTR("POST"), uri, parameters,
      headers);
  sendContentPayload(result, data);
  skipHeader(result);
  return result;
}

FILE*
HTTPClientWiFi::putURI(char* uri, char* data)
{
  return putURI(uri, NULL, data, NULL);
}
FILE*
HTTPClientWiFi::putURI(char* uri, http_client_parameter parameters[], char* data)
{
  return putURI(uri, parameters, data, NULL);
}

FILE*
HTTPClientWiFi::putURI(char* uri, http_client_parameter parameters[], char* data,
    http_client_parameter headers[])
{
  FILE* result = openClientFile();
  if (result == NULL)
    {
      return NULL;
    }
  sendUriAndHeaders(result, this->hostName, PSTR("PUT"), uri, parameters,
      headers);
  sendContentPayload(result, data);
  skipHeader(result);
  return result;
}

int
HTTPClientWiFi::getLastReturnCode(void)
{
  return lastReturnCode;
}

void
HTTPClientWiFi::debug(char debugOn)
{
  this->debugCommunication = debugOn;
}

void
HTTPClientWiFi::setEncoding(FILE* stream, char encode, char encodeReserved)
{
  http_stream_udata* udata = (http_stream_udata*) fdev_get_udata(stream);
  if (encode == 0)
    {
      udata->encode = 0;
    }
  else
    {
      if (encodeReserved)
        {
          udata->encode = URI_ENCODE | URI_ENCODE_RESERVED;
        }
      else
        {
          udata->encode = URI_ENCODE;
        }
    }
}

FILE*
HTTPClientWiFi::openClientFile()
{
  FILE* result = fdevopen(clientWrite, clientRead);
  if (result == NULL)
    {
      return NULL;
    }
  http_stream_udata* udata = (http_stream_udata*) malloc(
      sizeof(http_stream_udata));
  fdev_set_udata(result,udata);
  udata->client = this;
  udata->encode = 0;
  if (connected())
    {
      stop();
    }
  if (connect(ip,port))
    {
      return result;
    }
  else
    {
      closeStream(result);
      return NULL;
    }
}

char
HTTPClientWiFi::sendUriAndHeaders(FILE* stream, char* hostName, const char* requestType, char* uri,
    http_client_parameter parameters[], http_client_parameter headers[])
{
  fprintf_P(stream, requestType, uri);
  fprintf_P(stream, PSTR(" "), uri);
  //encode but use reserved characters
  HTTPClientWiFi::setEncoding(stream, 1, 0);
  fprintf_P(stream, PSTR("%s"), uri);
  if (parameters != NULL)
    {
      fprintf_P(stream, PSTR("?"));
      char parameter_number = 0;
      http_client_parameter* parameter = &parameters[0];
      while (parameter->name != NULL)
        {
          if (parameter_number > 0)
            {
              fprintf_P(stream, PSTR("&"));
            }
          HTTPClientWiFi::setEncoding(stream, 1, 1);
          fprintf_P(stream, PSTR("%s"), parameter->name);
          HTTPClientWiFi::setEncoding(stream, 1, 0);
          fprintf_P(stream, PSTR("="));
          HTTPClientWiFi::setEncoding(stream, 1, 1);
          if (parameter->value != NULL)
            {
              fprintf_P(stream, PSTR("%s"), parameter->value);
            }
          HTTPClientWiFi::setEncoding(stream, 1, 0);
          parameter_number++;
          parameter = &parameters[parameter_number];
        }
    }
  HTTPClientWiFi::setEncoding(stream, 0, 0);
  fprintf_P(stream, PSTR(
      " HTTP/1.1\nHost: %s\nAccept: */*\nConnection: close\n"), hostName);
  //is there an additional header?
  if (headers != NULL)
    {
      char headerNumber = 0;
      while (headers[headerNumber].name != NULL)
        {
          if (headers[headerNumber].value != NULL)
            {
              fprintf_P(stream, PSTR("%s: %s\n"), headers[headerNumber].name,
                  headers[headerNumber].value);
				headerNumber++;
            }
        }
    }
  return 0;
}

char
HTTPClientWiFi::sendContentPayload(FILE* stream, char* data)
{
  //calculate the content length
  int content_length = 0;
  if (data != NULL)
    {
      content_length = strlen(data);
    }
  fprintf_P(stream, PSTR("Content-Length: %i\n"), content_length);
  //ok finished header
  fprintf_P(stream, PSTR("\n"));
  //now sending data
  if (content_length > 0)
    {
      fprintf_P(stream, PSTR("%s\n"), data);
    }
  fprintf_P(stream, PSTR("\n"));
  return 0;
}

int
HTTPClientWiFi::clientWrite(char byte, FILE* stream)
{
  if (stream == NULL)
    {
      return EOF;
    }
  http_stream_udata* udata = (http_stream_udata*) fdev_get_udata(stream);
  HTTPClientWiFi* client = udata->client;
  if (client->connected() == 0)
    {
      closeStream(stream);
      return EOF;
    }
  if (udata->encode == 0)
    {
      client->write(byte);
      if (client->debugCommunication)
        {
          Serial.print(byte);
        }
    }
  else
    {
      if (URI_ALLOWED(byte) || ((URI_RESERVED(byte) && (udata->encode
          & URI_ENCODE_RESERVED) == 0)))
        {
          client->write(byte);
          if (client->debugCommunication)
            {
              Serial.print(byte);
            }
        }
      else
        {
          char encoded[4] =
            { 0, 0, 0 };
          sprintf(encoded, "%%%2x", byte);
          // Write only the first three bytes, not the trailing null
          for (char i = 0; i < 3; i++)
            {
              client->write(encoded[i]);
              if (client->debugCommunication)
                {
                  Serial.print(encoded[i]);
                }
            }
        }
    }
  return 0;
}

int
HTTPClientWiFi::clientRead(FILE* stream)
{
  if (stream == NULL)
    {
      return EOF;
    }
  http_stream_udata* udata = (http_stream_udata*) fdev_get_udata(stream);
  HTTPClientWiFi* client = udata->client;
  if (!client->connected())
    {
      return EOF;
    }
  //block until we got a byte
  while (client->available() == 0)
    {
      if (client->connected() == 0)
        {
          return EOF;
        }
    };
  int result = client->read();
  if (result == EOF)
    {
      return EOF;
    }
  if (client->debugCommunication)
    {
      Serial.print((byte) result);
    }
  //as long as we do not read encoded or it is no % everything is ok
  if (udata->encode == 0 || result != '%')
    {
      return result;
    }
  else
    {
      //block until we got the needed bytes
      while (client->available() >= 2)
        {
          if (client->connected() == 0)
            {
              return EOF;
            }
        };
      char return_value = 0;
      for (char i = 0; i < 2; i++)
        {
          result = client->read();
          if (result == EOF)
            {
              return EOF;
            }
          else if (result >= 'A' && result <= 'Z')
            {
              return_value += (1 - i) * 16 * (result - 'A');
            }
          else if (result >= 'a' && result <= 'z')
            {
              return_value += (1 - i) * 16 * (result - 'a');
            }
          else if (result >= '0' && result <= '9')
            {
              return_value += (1 - i) * 16 * (result - '0');
            }
        }
      return return_value;
    }
}

int
HTTPClientWiFi::skipHeader(FILE* stream)
{
  //skip over the header
  fscanf_P(stream, PSTR("HTTP/1.1 %i"), lastReturnCode);
  static int inByte = 0;
  static int lastByte = 0;
  while (!(inByte == '\n' && lastByte == '\n'))
    {
      //by that we ignore \r
      if (inByte != '\r')
        {
          lastByte = inByte;
        }
      inByte = fgetc(stream);
      if (inByte == EOF)
        {
          //hmm, an error occured - lets just end this
          HTTPClientWiFi::closeStream(stream);
          return NULL;
        }
    }
  return 0;
}

void
HTTPClientWiFi::closeStream(FILE* stream)
{
  if (stream != NULL)
    {
      http_stream_udata* udata = (http_stream_udata*) fdev_get_udata(stream);
      if (udata->client->connected())
        {
          udata->client->stop();
        }
      free(udata);
      fclose(stream);
    }
}
