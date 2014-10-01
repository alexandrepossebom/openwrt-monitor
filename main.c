/*
 * Copyright 2014 Alexandre Possebom
 * This file is part of OpenWRT Monitor.
 *
 *   OpenWRT Monitor is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Foobar is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with OpenWRT Monitor.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <time.h>
#include <ctype.h>

#define MAXMATCH 5
#define MAXCLIENTS 256

struct client {
    time_t connect;
    char wlan[10];
    char wlanName[10];
    char mac[22];
    char name[50];
};

struct hosts {
    char mac[22];
    char name[50];
};

struct client cli[MAXCLIENTS];
struct hosts host[MAXCLIENTS];

time_t lastrun;

int needsRewrite = 0;

void getName(char *mac,char *name){
    int i;
    for (i=0;i<MAXCLIENTS;i++){
        if(strlen(host[i].mac) == 0){
            break;
        }else if(strcmp(host[i].mac,mac) == 0){
            strcpy(name,host[i].name);
            return;
        }
    }
    strcpy(name,mac);
}

void initNames(){
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen("/root/hosts", "r");
    if (fp == NULL){
        return;
    }
    int i = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        char *pattern = "(.*);(.*)\n";
        regex_t emma;
        regmatch_t matches[MAXMATCH];
        char name[50];
        char mac[20];
        int numchars;
        int status;
        regcomp(&emma,pattern,REG_ICASE | REG_EXTENDED);
        status = regexec(&emma,line,MAXMATCH,matches,0);
        if(status == 0){
            numchars = (int)matches[2].rm_eo - (int)matches[2].rm_so;
            strncpy(name,line+matches[2].rm_so,numchars);
            name[numchars] = '\0';
            numchars = (int)matches[1].rm_eo - (int)matches[1].rm_so;
            strncpy(mac,line+matches[1].rm_so,numchars);
            mac[numchars] = '\0';
            regfree(&emma);
            strcpy(host[i].name,name);
            strcpy(host[i].mac,mac);
            i++;
        }
    }
    pclose(fp);
}

void logToFile(const char *str){
    printf("%s\n", str);
    FILE *fp;

    fp = fopen("/www/stats/wifi.txt", "a");
    if (fp == NULL) {
        printf("I couldn't open logfile for writing.\n");
        return;
    }

    fprintf(fp, "%s \n", str);
    fclose(fp);
}

void wlanToHuman(char * wlan){
    if(!strcmp(wlan,"wlan0-1")){
        strcpy(wlan,"[FREE]");
    }else if(!strcmp(wlan,"wlan0")){
        strcpy(wlan,"[ 2G ]");
    }else if(!strcmp(wlan,"wlan1")){
        strcpy(wlan,"[ 5G ]");
    }else{
        char tmp[10];
        strcpy(tmp,wlan);
        sprintf(wlan,"[%-3s]",tmp);
    }
}

void secondsToHuman(char * buffer,long elapsed){
    int days;
    int hours;
    int minutes;
    int seconds;

    days = elapsed / 86400;
    elapsed = elapsed % 86400;
    hours = elapsed / 3600;
    elapsed = elapsed % 3600;
    minutes = elapsed / 60;
    seconds = elapsed % 60;

    if(days > 0){
        sprintf (buffer, "%02dd%02dh%02dm%02ds",days,hours,minutes,seconds);
    }else if(hours > 0){
        sprintf (buffer, "   %02dh%02dm%02ds",hours,minutes,seconds);
    }else if(minutes > 0){
        sprintf (buffer, "      %02dm%02ds",minutes,seconds);
    }else{
        sprintf (buffer, "         %02ds",seconds);
    }
}

void parse(char *line,char *mac,char *wlan){
    char *pattern = "hostapd: ([a-z]+[0-9]+[-]*[0-9]*): STA (([0-9A-F]{2}[:-]){5}([0-9A-F]{2}))";
    regex_t emma;
    regmatch_t matches[MAXMATCH];
    int numchars;
    regcomp(&emma,pattern,REG_ICASE | REG_EXTENDED);
    regexec(&emma,line,MAXMATCH,matches,0);
    numchars = (int)matches[1].rm_eo - (int)matches[1].rm_so;
    strncpy(wlan,line+matches[1].rm_so,numchars);
    wlan[numchars] = '\0';
    numchars = (int)matches[2].rm_eo - (int)matches[2].rm_so;
    strncpy(mac,line+matches[2].rm_so,numchars);
    mac[numchars] = '\0';
    regfree(&emma);
}

void parseHosts(char *line){
    char *pattern = "(([0-9A-F]{2}[:-]){5}([0-9A-F]{2})) (.*)\n";
    char mac[22];
    char name[50];
    regex_t emma;
    regmatch_t matches[MAXMATCH];
    int i;
    int status;

    int numchars;
    regcomp(&emma,pattern,REG_ICASE | REG_EXTENDED);
    status = regexec(&emma,line,MAXMATCH,matches,0);
    if(status == 1){
        return;
    }

    numchars = (int)matches[1].rm_eo - (int)matches[1].rm_so;
    strncpy(mac,line+matches[1].rm_so,numchars);
    mac[numchars] = '\0';
    numchars = (int)matches[4].rm_eo - (int)matches[4].rm_so;
    strncpy(name,line+matches[4].rm_so,numchars);
    name[numchars] = '\0';
    regfree(&emma);

    if(strlen(name)==0){
        return;
    }

    for (i=0;i<MAXCLIENTS;i++){
        if(strlen(host[i].mac) == 0){
            strcpy(host[i].name,name);
            strcpy(host[i].mac,mac);
            needsRewrite = 1;
            break;
        }else if(!strcmp(mac,host[i].mac) && strcmp(host[i].name,name)){
            strcpy(host[i].name,name);
            strcpy(host[i].mac,mac);
            needsRewrite = 1;
            break;
        }else if(!strcmp(mac,host[i].mac) && !strcmp(host[i].name,name)){
            break;
        }
    }

}

void writeHosts(){
    if(!needsRewrite){
        return;
    }
    needsRewrite = 0;

    int i;
    FILE *fp;
    fp = fopen("/root/hosts", "w");
    if (fp == NULL) {
        return;
    }
    for (i=0;i<MAXCLIENTS;i++){
        if(strlen(host[i].mac) == 0){
            break;
        }
        fprintf(fp, "%s;%s\n", host[i].mac,host[i].name);
    }

    fclose(fp);
}


void disconnect(const char *mac,const char *wlan){
    int i;
    for (i=0;i< MAXCLIENTS;i++){
        if(cli[i].connect > 0 && !strcmp(cli[i].mac,mac) && !strcmp(cli[i].wlan,wlan)){
            time_t t = time(0);
            long diff = (long) difftime(t,cli[i].connect);

            char date[100];
            strftime(date, 100, "%d-%m %H:%M", localtime (&t));

            char buffer [100];

            char dateStr[100];
            secondsToHuman(dateStr,diff);

            sprintf(buffer,"%s [<--] %s %-25s %s",date,cli[i].wlanName,cli[i].name,dateStr);

            logToFile(buffer);

            cli[i].connect = 0;
            break;
        }
    }
}

void dump(){
    int i;
    char date[100];
    time_t t = time(0);
    strftime(date, 100, "%d-%m %H:%M", localtime (&t));
    for (i=0;i< MAXCLIENTS;i++){
        if(cli[i].connect > 0){
            long diff = (long) difftime(t,cli[i].connect);
            char buffer [100];
            char dateStr[100];
            secondsToHuman(dateStr,diff);
            sprintf(buffer,"%s DUMP  %s %-25s %s",date,cli[i].wlanName,cli[i].name,dateStr);
            logToFile(buffer);
        }
    }
}

void connect(char *mac,char *wlan){
    int i;
    disconnect(mac,wlan);
    char name[50];
    char date[100];
    char buffer[100];

    for (i=0;i<MAXCLIENTS;i++){
        if(cli[i].connect == 0){
            time(&cli[i].connect);
            strcpy(cli[i].mac,mac);
            strcpy(cli[i].wlan,wlan);
            wlanToHuman(wlan);
            strcpy(cli[i].wlanName,wlan);
            getName(mac,name);
            strcpy(cli[i].name,name);
            break;
        }
    }
    strftime(date, 100, "%d-%m %H:%M", localtime(&cli[i].connect));
    sprintf(buffer,"%s [-->] %s %-25s", date,wlan,name);
    logToFile(buffer);
}

void checkHosts(){
    time_t t = time(0);
    int i;
    long diff = (long) difftime(t,lastrun);
    if(diff > 300){
        writeHosts();
        printf("> %ld\n",diff);
        lastrun = time(0);

        for (i=0;i< MAXCLIENTS;i++){
            if(cli[i].connect > 0){
                FILE *fp;
                char path[1035];
                char buffer[100];

                sprintf(buffer,"iw dev %s station get %s 2>&1",cli[i].wlan , cli[i].mac);

                fp = popen(buffer, "r");
                if (fp == NULL) {
                    printf("Failed to run command\n" );
                    return;
                }

                if (fgets(path, sizeof(path)-1, fp) != NULL) {
                    if (strstr(path, "failed") != NULL) {
                        printf("disconnect %s %s \n", cli[i].mac,cli[i].wlan);
                        disconnect(cli[i].mac,cli[i].wlan);
                    }
                }
                pclose(fp);
            }
        }
    }
}

void process(char *str){
    char mac[19];
    char wlan[10];
    if (strstr(str, " authenticated") != NULL) {
        parse(str,mac,wlan);
        connect(mac,wlan);
    } else if (strstr(str, "deauthenticated") != NULL) {
        parse(str,mac,wlan);
        disconnect(mac,wlan);
    } else if (strstr(str, "user.notice root: DUMP") != NULL){
        dump();
    } else if (strstr(str, "DHCPACK") != NULL){
        parseHosts(str);
    } else {
        checkHosts();
    }
}

int main () {
    lastrun = time(0);

    initNames();

    FILE *fp;
    char path[1035];

    fp = popen("logread -f", "r");
    if (fp == NULL) {
        printf("Failed to run command\n" );
        return EXIT_FAILURE;
    }

    while (fgets(path, sizeof(path)-1, fp) != NULL) {
        process(path);
    }

    pclose(fp);
    return EXIT_SUCCESS;
}
