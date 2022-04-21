#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "openssl/md5.h"
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
// kompilowac: gcc Lamacz_hasel.c -lssl -lcrypto -pthread -lm
/////////////////////////////ZMIENNE /////////////////////////////////////////////////////////////////////////
char **slownik;
int lines_slownik = 0;

char **hasla;
int ilosc_hasel = 0;

const char znaki[] = "0123456789!?@#$^&*()-_+:<>";

struct
{
    pthread_mutex_t mutex; //wspolnu mutex
    int liczba_zlamanych;
    char **zlamane_hasla;
    char **zlamane_hasla_md5;
    int *znalezione; // Jesli ktorys element=: 0=nieznalezione, 1=znazione przez producenta, 2=odczytane

} shared = {PTHREAD_MUTEX_INITIALIZER};

///////////////////////////////////////////////Sygnaly, jednolinijkowe podsumowanie///////////////////////////////////////////
void reakcja_SIGHUP(int syg)
{
    printf("DLA OBECNEGO PLIKU HASEL ZLAMANO %d NA %d HASEL\n", shared.liczba_zlamanych, ilosc_hasel);
}

void reakcja_SIGTERM(int syg)
{
    pthread_mutex_unlock(&shared.mutex);
    pthread_exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////KODOWANIE NA MD5///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

void to_md5(char slowo[], char *linia)
{

    unsigned char digest[MD5_DIGEST_LENGTH];
    char mdString[32];
    MD5((unsigned char *)slowo, strlen(slowo), (unsigned char *)&digest);

    for (int i = 0; i < 16; i++)
        sprintf(&mdString[i * 2], "%02x", (unsigned int)digest[i]);
    //printf("md5 digest: %s\n", mdString);
    strcpy(linia, mdString);
}

//--------------------------------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////////
//////////////////////////// WCZYTYWANIE DANYCH ///////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

int wczytaj_hasla(char nazwa_pliku[])
{
    char *line = NULL; //wskaznik na string
    FILE *fphasla;     // file pointer
    size_t len = 0;    // pomocnicze
    char ch = 'A';
    char ch_poprzedni;
    ilosc_hasel = 0;
  
    fphasla = fopen(nazwa_pliku, "r");
    if (fphasla == NULL)
        return (EXIT_FAILURE);
    while (!feof(fphasla))
    {
        ch_poprzedni = ch;
        ch = fgetc(fphasla);
        if (ch != '\n' && ch_poprzedni == '\n' && ch != ' ')
        {
            ilosc_hasel++;
        }
    }
    if (ilosc_hasel > 0 && ch_poprzedni != '\n')
    {
        ilosc_hasel++;
    }

    hasla = malloc(ilosc_hasel * sizeof(char *));                    //zarzerwowanie odpowiednie ilosci lini na wczytywane hasla w md5
    shared.zlamane_hasla_md5 = malloc(ilosc_hasel * sizeof(char *)); //zarzerwowanie odpowiednie ilosci lini na odgadywane hasla w md5
    shared.znalezione = malloc(ilosc_hasel * sizeof(int));           //zarzerwowanie odpowiednie ilosci lini na oznaczanie odnalezionych hasel
    shared.zlamane_hasla = malloc(ilosc_hasel * sizeof(char *));     //zarzerwowanie odpowiedniej ilosci lini na oznaczanie odnalezionych hasel
    fclose(fphasla);

    //Zapisanie lini do tablicy
    fphasla = fopen(nazwa_pliku, "r");

    if (fphasla == NULL)
        return (EXIT_FAILURE);

    int pom = 0;
    while ((getline(&line, &len, fphasla)) != -1) //odczytywanie lini
    {
        if (line[0] != '\n' && line[0] != ' ')
        {
            hasla[pom] = malloc((strcspn(line, "\n") + 1) * sizeof(char));                    // zalokowanie odpowiedniej ilosci miejsca na wczytane hasla w md5(33 znaki)
            shared.zlamane_hasla_md5[pom] = malloc((strcspn(line, "\n") + 1) * sizeof(char)); // zalokowanie odpowiedniej ilosci miejsca na zlamane hasla w md5(33 znaki)
            strcpy(hasla[pom], strtok(line, "\n"));                                           // skopiowanie hasla do poamieci dynamicznej
            //printf("%d Haslo: %s\n", pom, hasla[pom]);
            pom++;
        }
    }
    printf("Wczytano %d hasla do zlamania \n", ilosc_hasel);
    return 0;
}

int wczytaj_slownik(char nazwa_pliku[])
{
    char *line = NULL; //wskaznik na string
    FILE *fpslownik;   // file pointer
    size_t len = 0;    // pomocnicze
    char ch = 'A';
    char ch_poprzedni;

    fpslownik = fopen(nazwa_pliku, "r");
    if (fpslownik == NULL)
        return (EXIT_FAILURE);

    //liczenie ilosci lini///
    while (!feof(fpslownik))
    {
        ch_poprzedni = ch;
        ch = fgetc(fpslownik);
        if (ch != '\n' && ch_poprzedni == '\n' && ch != ' ') //warunek umozliwiajacy pomijanie linijek z spacjami lub samymi enterami
        {
            lines_slownik++;
        }
    }
    if (lines_slownik > 0 && ch_poprzedni != '\n') // warunek sprawdzajacy czy na koncu pliku jest enter czy nie, dzieki czemu odpowiednio zliczane sa linie
    {
        lines_slownik++;
    }

    printf("Wczytano %d lini ze slownika \n", lines_slownik);
    slownik = malloc(lines_slownik * sizeof(char *)); //zarzerwowanie odpowiednie ilosci lini
    fclose(fpslownik);

    //Zapisanie lini do tablicy
    fpslownik = fopen(nazwa_pliku, "r");
    if (fpslownik == NULL)
        return (EXIT_FAILURE);
    int pom = 0;
    while ((getline(&line, &len, fpslownik)) != -1) //odczytywanie lini
    {
        if (line[0] != '\n' && line[0] != ' ')
        {
            // printf("Dlugosc slowa %zu:\n", strcspn(line, "\n"));
            slownik[pom] = malloc((strcspn(line, "\n") + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
            strcpy(slownik[pom], strtok(line, "\n"));                        // skopiowanie slowa do poamieci dynamicznej z obcieceim \n
            pom++;
        }
    }
    return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////PRODUCENCI KOMBINACJI JEDNEGO MALEGO SLOWA ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *produce_small_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    
    char work_md5[33];
    char *ptr0 = NULL;

    //for (;;) //nieskonczona petla
    // {
        
    for (int i = 0; i < lines_slownik; i++)
    {
        ptr0 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
        ptr0[0] = '\0';
        strcpy(ptr0, slownik[i]);
        //zmiana znakow na mniejsze
        for (int len = 0; len < strlen(ptr0); len++)
        {
            ptr0[len] = tolower(ptr0[len]);
        }
        
        to_md5(ptr0, work_md5);
        for (int a = 0; a < ilosc_hasel; a++)
        {
            pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
            if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
            {
                pthread_mutex_unlock(&shared.mutex);
                printf("Koniec pracy producenta\n");
                return (NULL); // zakonczenie pracy
            }
            if (shared.znalezione[a] == 0)
            {
                if (strcmp(work_md5, hasla[a]) == 0) // jesli uda sie zlamac haslo
                {

                    shared.zlamane_hasla[a] = malloc((strlen(ptr0) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                    strcpy(shared.zlamane_hasla[a], ptr0);                               //zapisanie zlamanego hasla
                    strcpy(shared.zlamane_hasla_md5[a], work_md5);
                    shared.liczba_zlamanych++;
                    shared.znalezione[a] = 1;
                }
            }
            pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
        }
        free(ptr0);
    }
    //}
    printf("Koniec pracy producenta \n");
    return (NULL); // zakonczenie pracy
}

void *produce_something_plus_small_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1;
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpwiedniej ilosci miejsca
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie miejsca na tablice kombinacji
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                strcpy(ptr1, slownik[i]);
                //zmiana znakow na mniejsze
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                strcat(ptr0, ptr1);
                //printf("%d. cos+slowo:%s.\n", d, ptr0);

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie bylo jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdz czy uda sie zlamac, jesli tak...
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wyniku
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_small_word_plus_something(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                          //zmiana na mniejsze litery
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                strcat(ptr0, ptr1);

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                //Przypisywanie odpowiednich znakow specjalnych do slowa
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_something_plus_small_word_plus_something_sides_lenght_symetric(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33]; // zakodowane haslo w md5
    //wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 2; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja = 0;
    int kombinacja_pom;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {
            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //alokwacja odpowiedniej liczby miejsca na kombinacje
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  //zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpwiedniej liczby miejsca na znaki specjalne
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= LS / 2; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }
                //skopiowanie slowa ze slownika
                strcpy(ptr1, slownik[i]);
                //zmiana na mniejsze litery
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                //kopiowanie do glownego hasla
                strcat(ptr0, ptr1);

                for (; LSpom >= 0; LSpom--) //przypisywanie dalszych specjalnych znakow
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);               // konwersja na md5
                for (int a = 0; a < ilosc_hasel; a++) // sprawdzenie czy kombinacja pasuje do ktoregos z hasel
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta\n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli jeszcze nie zlamano
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdzenie czy wyprodukowane haslo pasuje z haslem ktore chcemy zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie zlamanego hasla
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1; // ocznaczenie jako wyprodukowane
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS = LS + 2;
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////PRODUCENCI KOMBINACJI JEDNEGO DUZEGO SLOWA ///////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *produce_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    char *ptr0 = NULL;

    //for (;;) //nieskonczona petla
    // {
    for (int i = 0; i < lines_slownik; i++)
    {
        ptr0 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
        ptr0[0] = '\0';
        strcpy(ptr0, slownik[i]);
        //zmiana znakow na mniejsze
        for (int len = 0; len < strlen(ptr0); len++)
        {
            ptr0[len] = toupper(ptr0[len]);
        }

        to_md5(ptr0, work_md5);
        for (int a = 0; a < ilosc_hasel; a++)
        {
            pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
            if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
            {
                pthread_mutex_unlock(&shared.mutex);
                printf("Koniec pracy producenta\n");
                return (NULL); // zakonczenie pracy
            }
            if (shared.znalezione[a] == 0)
            {
                if (strcmp(work_md5, hasla[a]) == 0) // jesli uda sie zlamac haslo
                {

                    shared.zlamane_hasla[a] = malloc((strlen(ptr0) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                    strcpy(shared.zlamane_hasla[a], ptr0);                               //zapisanie zlamanego hasla
                    strcpy(shared.zlamane_hasla_md5[a], work_md5);
                    shared.liczba_zlamanych++;
                    shared.znalezione[a] = 1;
                }
            }
            pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
        }
        free(ptr0);
    }
    //}
    printf("Koniec pracy producenta \n");
    return (NULL); // zakonczenie pracy
}

void *produce_something_plus_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1;
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpwiedniej ilosci miejsca
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie miejsca na tablice kombinacji
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                strcpy(ptr1, slownik[i]);
                //zmiana znakow na wieksze
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = toupper(ptr1[len]);
                }
                strcat(ptr0, ptr1);
                //printf("%d. cos+slowo:%s.\n", d, ptr0);

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie bylo jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdz czy uda sie zlamac, jesli tak...
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wyniku
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_big_word_plus_something(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                          //zmiana na wieksze litery
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = toupper(ptr1[len]);
                }
                strcat(ptr0, ptr1);

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                //Przypisywanie odpowiednich znakow specjalnych do slowa
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_something_plus_big_word_plus_something_sides_lenght_symetric(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33]; // zakodowane haslo w md5
    //wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 2; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja = 0;
    int kombinacja_pom;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {
            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //alokwacja odpowiedniej liczby miejsca na kombinacje
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  //zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpwiedniej liczby miejsca na znaki specjalne
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= LS / 2; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }
                //skopiowanie slowa ze slownika
                strcpy(ptr1, slownik[i]);
                //zmiana na wieksze litery
                for (int len = 0; len < strlen(ptr1); len++)
                {
                    ptr1[len] = toupper(ptr1[len]);
                }
                //kopiowanie do glownego hasla
                strcat(ptr0, ptr1);

                for (; LSpom >= 0; LSpom--) //przypisywanie dalszych specjalnych znakow
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);               // konwersja na md5
                for (int a = 0; a < ilosc_hasel; a++) // sprawdzenie czy kombinacja pasuje do ktoregos z hasel
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli jeszcze nie zlamano
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdzenie czy wyprodukowane haslo pasuje z haslem ktore chcemy zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie zlamanego hasla
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1; // ocznaczenie jako wyprodukowane
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS = LS + 2;
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------//

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////PRODUCENCI KOMBINACJI PIERWSZEJ LITERY SLOWA DUZEJ RESZTY MALEJ /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *produce_first_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    char *ptr0 = NULL;

    //for (;;) //nieskonczona petla
    // {
    for (int i = 0; i < lines_slownik; i++)
    {
        ptr0 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
        ptr0[0] = '\0';
        strcpy(ptr0, slownik[i]);
        //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
        if (strlen(ptr0) > 0)
        {
            ptr0[0] = toupper(ptr0[0]);
        }
        for (int len = 1; len < strlen(ptr0); len++)
        {
            ptr0[len] = tolower(ptr0[len]);
        }

        to_md5(ptr0, work_md5);
        for (int a = 0; a < ilosc_hasel; a++)
        {
            pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
            if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
            {
                pthread_mutex_unlock(&shared.mutex);
                printf("Koniec pracy producenta \n");
                return (NULL); // zakonczenie pracy
            }
            if (shared.znalezione[a] == 0)
            {
                if (strcmp(work_md5, hasla[a]) == 0) // jesli uda sie zlamac haslo
                {

                    shared.zlamane_hasla[a] = malloc((strlen(ptr0) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                    strcpy(shared.zlamane_hasla[a], ptr0);                               //zapisanie zlamanego hasla
                    strcpy(shared.zlamane_hasla_md5[a], work_md5);
                    shared.liczba_zlamanych++;
                    shared.znalezione[a] = 1;
                }
            }
            pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
        }

        free(ptr0);
    }
    //}
    printf("Koniec pracy producenta\n");
    return (NULL); // zakonczenie pracy
}

void *produce_something_plus_first_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1;
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpwiedniej ilosci miejsca
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie miejsca na tablice kombinacji
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                strcpy(ptr1, slownik[i]);
                //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
                if (strlen(ptr1) > 0)
                {
                    ptr1[0] = toupper(ptr1[0]);
                }
                for (int len = 1; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                strcat(ptr0, ptr1);
                //printf("%d. cos+slowo:%s.\n", d, ptr0);

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie bylo jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdz czy uda sie zlamac, jesli tak...
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wyniku
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_first_big_word_plus_something(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 1; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {

            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  // zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';
                strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                          //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
                if (strlen(ptr1) > 0)
                {
                    ptr1[0] = toupper(ptr1[0]);
                }
                for (int len = 1; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                strcat(ptr0, ptr1);

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                //Przypisywanie odpowiednich znakow specjalnych do slowa
                for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);
                for (int a = 0; a < ilosc_hasel; a++)
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1;
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS++;
    }
}

void *produce_something_plus_first_big_word_plus_something_sides_lenght_symetric(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33]; // zakodowane haslo w md5
    //wskazniki na slowa
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 2; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja = 0;
    int kombinacja_pom;

    for (;;) //nieskonczona petla
    {
        for (int i = 0; i < lines_slownik; i++)
        {
            ptr0 = malloc((LS + strlen(slownik[i]) + 1) * sizeof(char)); //alokwacja odpowiedniej liczby miejsca na kombinacje
            ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
            for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
            {
                kombinacja = d;                                  //zapisanie numeru kombinacji
                tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpwiedniej liczby miejsca na znaki specjalne
                for (int c = 0; c < LS; c++)
                {
                    tablica_kombinacji[c] = malloc(sizeof(int));
                    *tablica_kombinacji[c] = 0;
                }
                ptr0[0] = '\0';

                // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                {
                    *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                    kombinacja = kombinacja / strlen(znaki);
                }
                LSpom = LS;
                for (LSpom = LSpom - 1; LSpom >= LS / 2; LSpom--)
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }
                //skopiowanie slowa ze slownika
                strcpy(ptr1, slownik[i]);

                //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
                if (strlen(ptr1) > 0)
                {
                    ptr1[0] = toupper(ptr1[0]);
                }
                for (int len = 1; len < strlen(ptr1); len++)
                {
                    ptr1[len] = tolower(ptr1[len]);
                }
                //kopiowanie do glownego hasla
                strcat(ptr0, ptr1);

                for (; LSpom >= 0; LSpom--) //przypisywanie dalszych specjalnych znakow
                {

                    strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                }

                to_md5(ptr0, work_md5);               // konwersja na md5
                for (int a = 0; a < ilosc_hasel; a++) // sprawdzenie czy kombinacja pasuje do ktoregos z hasel
                {
                    pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                    if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                    {
                        pthread_mutex_unlock(&shared.mutex);
                        printf("Koniec pracy producenta \n");
                        return (NULL); // zakonczenie pracy
                    }
                    if (shared.znalezione[a] == 0) // jesli jeszcze nie zlamano
                    {
                        if (strcmp(work_md5, hasla[a]) == 0) // sprawdzenie czy wyprodukowane haslo pasuje z haslem ktore chcemy zlamac
                        {

                            shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                            strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie zlamanego hasla
                            strcpy(shared.zlamane_hasla_md5[a], work_md5);
                            shared.liczba_zlamanych++;
                            shared.znalezione[a] = 1; // ocznaczenie jako wyprodukowane
                        }
                    }
                    pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                }
                for (int h = 0; h < LS; h++)
                {
                    free(tablica_kombinacji[h]);
                }
                free(tablica_kombinacji);
            }
            free(ptr1);
            free(ptr0);
        }
        LS = LS + 2;
    }
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////PRODUCENCI DWU WYRAZOWI//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void *produce_first_big_word_plus_something_plus_first_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr2 = NULL;
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 0; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int os = 0; os < lines_slownik; os++)
        {
            for (int i = 0; i < lines_slownik; i++)
            {

                ptr0 = malloc((LS + strlen(slownik[i]) + strlen(slownik[os]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
                ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
                ptr2 = malloc((strlen(slownik[os]) + 1) * sizeof(char));
                for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
                {
                    kombinacja = d;                                  // zapisanie numeru kombinacji
                    tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                    for (int c = 0; c < LS; c++)
                    {
                        tablica_kombinacji[c] = malloc(sizeof(int));
                        *tablica_kombinacji[c] = 0;
                    }
                    ptr0[0] = '\0';
                    strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                              //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
                    if (strlen(ptr1) > 0)
                    {
                        ptr1[0] = toupper(ptr1[0]);
                    }
                    for (int len = 1; len < strlen(ptr1); len++)
                    {
                        ptr1[len] = tolower(ptr1[len]);
                    }
                    strcat(ptr0, ptr1);

                    // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                    for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                    {
                        *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                        kombinacja = kombinacja / strlen(znaki);
                    }
                    LSpom = LS;
                    //Przypisywanie odpowiednich znakow specjalnych do slowa
                    for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                    {

                        strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                    }

                    strcpy(ptr2, slownik[os]); //kompiowanie slowa ze slownika
                                               //Zmaiana znakow na pierwsze wieksze, reszta mniejsze
                    if (strlen(ptr2) > 0)
                    {
                        ptr2[0] = toupper(ptr2[0]);
                    }
                    for (int len = 1; len < strlen(ptr2); len++)
                    {
                        ptr2[len] = tolower(ptr2[len]);
                    }
                    strcat(ptr0, ptr2);
                    to_md5(ptr0, work_md5);
                    for (int a = 0; a < ilosc_hasel; a++)
                    {
                        pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                        if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                        {
                            pthread_mutex_unlock(&shared.mutex);
                            printf("Koniec pracy producenta \n");
                            return (NULL); // zakonczenie pracy
                        }
                        if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                        {
                            if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                            {

                                shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                                strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                                strcpy(shared.zlamane_hasla_md5[a], work_md5);
                                shared.liczba_zlamanych++;
                                shared.znalezione[a] = 1;
                            }
                        }
                        pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                    }
                    for (int h = 0; h < LS; h++)
                    {
                        free(tablica_kombinacji[h]);
                    }
                    free(tablica_kombinacji);
                }
                free(ptr2);
                free(ptr1);
                free(ptr0);
            }
        }
        LS++;
    }
}

void *produce_big_word_plus_something_plus_big_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr2 = NULL;
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 0; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int os = 0; os < lines_slownik; os++)
        {
            for (int i = 0; i < lines_slownik; i++)
            {

                ptr0 = malloc((LS + strlen(slownik[i]) + strlen(slownik[os]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
                ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
                ptr2 = malloc((strlen(slownik[os]) + 1) * sizeof(char));
                for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
                {
                    kombinacja = d;                                  // zapisanie numeru kombinacji
                    tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                    for (int c = 0; c < LS; c++)
                    {
                        tablica_kombinacji[c] = malloc(sizeof(int));
                        *tablica_kombinacji[c] = 0;
                    }
                    ptr0[0] = '\0';
                    strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                              //Zmaiana znakow na pierwsze wieksze, reszta mniejsze

                    for (int len = 0; len < strlen(ptr1); len++)
                    {
                        ptr1[len] = toupper(ptr1[len]);
                    }
                    strcat(ptr0, ptr1);

                    // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                    for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                    {
                        *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                        kombinacja = kombinacja / strlen(znaki);
                    }
                    LSpom = LS;
                    //Przypisywanie odpowiednich znakow specjalnych do slowa
                    for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                    {

                        strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                    }

                    strcpy(ptr2, slownik[os]); //kompiowanie slowa ze slownika
                                               //Zmaiana znakow na pierwsze wieksze, reszta mniejsze

                    for (int len = 0; len < strlen(ptr2); len++)
                    {
                        ptr2[len] = toupper(ptr2[len]);
                    }
                    strcat(ptr0, ptr2);
                    to_md5(ptr0, work_md5);
                    for (int a = 0; a < ilosc_hasel; a++)
                    {
                        pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                        if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                        {
                            pthread_mutex_unlock(&shared.mutex);
                            printf("Koniec pracy producenta \n");
                            return (NULL); // zakonczenie pracy
                        }
                        if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                        {
                            if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                            {

                                shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                                strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                                strcpy(shared.zlamane_hasla_md5[a], work_md5);
                                shared.liczba_zlamanych++;
                                shared.znalezione[a] = 1;
                            }
                        }
                        pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                    }
                    for (int h = 0; h < LS; h++)
                    {
                        free(tablica_kombinacji[h]);
                    }
                    free(tablica_kombinacji);
                }
                free(ptr2);
                free(ptr1);
                free(ptr0);
            }
        }
        LS++;
    }
}

void *produce_small_word_plus_something_plus_small_word(void *arg)
{
    signal(SIGTERM, reakcja_SIGTERM);
    char work_md5[33];
    // wskazniki na slowa
    char *ptr2 = NULL;
    char *ptr1 = NULL;
    char *ptr0 = NULL;
    int LS = 0; //liczba specjalnych znakow
    int LSpom = 0;
    int **tablica_kombinacji = NULL;
    int kombinacja_pom;
    int kombinacja = 0; //numer kombinacji

    for (;;) //nieskonczona petla
    {
        for (int os = 0; os < lines_slownik; os++)
        {
            for (int i = 0; i < lines_slownik; i++)
            {

                ptr0 = malloc((LS + strlen(slownik[i]) + strlen(slownik[os]) + 1) * sizeof(char)); // zaalokowanie odpweidniej ilosci miejsca na slowa
                ptr1 = malloc((strlen(slownik[i]) + 1) * sizeof(char));
                ptr2 = malloc((strlen(slownik[os]) + 1) * sizeof(char));
                for (int d = 0; d < (pow(strlen(znaki), LS) * 1); d++) //dla wszystkich kombinacji
                {
                    kombinacja = d;                                  // zapisanie numeru kombinacji
                    tablica_kombinacji = malloc(sizeof(int *) * LS); // zaalokowanie odpowiedniej iloscji miejsca na kombinacjie
                    for (int c = 0; c < LS; c++)
                    {
                        tablica_kombinacji[c] = malloc(sizeof(int));
                        *tablica_kombinacji[c] = 0;
                    }
                    ptr0[0] = '\0';
                    strcpy(ptr1, slownik[i]); //kompiowanie slowa ze slownika
                                              //Zmaiana znakow na pierwsze wieksze, reszta mniejsze

                    for (int len = 0; len < strlen(ptr1); len++)
                    {
                        ptr1[len] = tolower(ptr1[len]);
                    }
                    strcat(ptr0, ptr1);

                    // Przeliczenie numeru kombinacji na odpowiednie komurki, operacja podobne ( nie taka sama) jak przeliczenie na kod binart ale od 0 do strlen(znak)
                    for (kombinacja_pom = 0; kombinacja > 0; kombinacja_pom++)
                    {
                        *tablica_kombinacji[kombinacja_pom] = kombinacja % strlen(znaki);
                        kombinacja = kombinacja / strlen(znaki);
                    }
                    LSpom = LS;
                    //Przypisywanie odpowiednich znakow specjalnych do slowa
                    for (LSpom = LSpom - 1; LSpom >= 0; LSpom--)
                    {

                        strncat(ptr0, &znaki[*tablica_kombinacji[LSpom]], 1);
                    }

                    strcpy(ptr2, slownik[os]); //kompiowanie slowa ze slownika
                                               //Zmaiana znakow na pierwsze wieksze, reszta mniejsze

                    for (int len = 0; len < strlen(ptr2); len++)
                    {
                        ptr2[len] = tolower(ptr2[len]);
                    }
                    strcat(ptr0, ptr2);
                    to_md5(ptr0, work_md5);
                    for (int a = 0; a < ilosc_hasel; a++)
                    {
                        pthread_mutex_lock(&shared.mutex);          //zablokowanie mutexu
                        if (shared.liczba_zlamanych >= ilosc_hasel) // jesli ilosc wyprodukowanych dobr jest wystaczajaca koniec
                        {
                            pthread_mutex_unlock(&shared.mutex);
                            printf("Koniec pracy producenta \n");
                            return (NULL); // zakonczenie pracy
                        }
                        if (shared.znalezione[a] == 0) // jesli haslo nie jest jeszcze zlamane
                        {
                            if (strcmp(work_md5, hasla[a]) == 0) // jesli udalo sie zlamac
                            {

                                shared.zlamane_hasla[a] = malloc((strlen(slownik[i]) + 1) * sizeof(char)); //zaalokowanie odpoweidniej ilosci miejsca
                                strcpy(shared.zlamane_hasla[a], ptr0);                                     // zapisanie wynikow
                                strcpy(shared.zlamane_hasla_md5[a], work_md5);
                                shared.liczba_zlamanych++;
                                shared.znalezione[a] = 1;
                            }
                        }
                        pthread_mutex_unlock(&shared.mutex); // odblokowanie muteksu
                    }
                    for (int h = 0; h < LS; h++)
                    {
                        free(tablica_kombinacji[h]);
                    }
                    free(tablica_kombinacji);
                }
                free(ptr2);
                free(ptr1);
                free(ptr0);
            }
        }
        LS++;
    }
}

void *consume(void *);

int main(int argc, char **argv)
{
    char *line = "NULL";
    size_t len = 0;
    ssize_t lineSize = 0;
    FILE *pom_file;

    if (argc != 3 && argc != 2)
    { //Jesli podano za malo argumentow
        printf("Bledne wywolanie programu. Proprawne wywolania: \n./a.out NazwaPlikuHasel NazwaPlikuSlownika\n./a.out NazwaPlikuHasel\n");
        return 1;
    }
    if (argc == 3)
    {
        if (wczytaj_hasla(argv[1]) != 0)
        {
            printf("Blad podczas wczytywania hasel\n");
            return 1;
        }
        if (wczytaj_slownik(argv[2]) != 0)
        {
            printf("Blad podczas wczytywania slownika\n");
            return 1;
        }
    }
    else
    {

        if (wczytaj_hasla(argv[1]) != 0)
        {
            printf("Blad podczas wczytywania hasel\n");
            return 1;
        }
        if (wczytaj_slownik("slownik.txt") != 0)
        { //Wczytanie domyslnego slownika
            printf("Blad podczas wczytywania slownika\n");
            return 1;
        }
    }

    // zerowanie pamieci wspolnej dla bezpieczenstwa
    shared.liczba_zlamanych = 0;
    for (int i = 0; i < ilosc_hasel; i++)
    {
        shared.znalezione[i] = 0;
    }

    //watki producentow
    pthread_t tid_produce_small_word;
    pthread_t tid_produce_something_plus_small_word;
    pthread_t tid_produce_small_word_plus_something;
    pthread_t tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric;

    pthread_t tid_produce_big_word;
    pthread_t tid_produce_something_plus_big_word;
    pthread_t tid_produce_big_word_plus_something;
    pthread_t tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric;

    pthread_t tid_produce_first_big_word;
    pthread_t tid_produce_something_plus_first_big_word;
    pthread_t tid_produce_first_big_word_plus_something;
    pthread_t tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric;

    //dwuwyrazowi
    pthread_t tid_produce_first_big_word_plus_something_plus_first_big_word;
    pthread_t tid_produce_big_word_plus_something_plus_big_word;
    pthread_t tid_produce_small_word_plus_something_plus_small_word;

    //watek konsumenta
    pthread_t tid_consume;

    //tworzenie producentow malych slow
    pthread_create(&tid_produce_small_word, NULL, produce_small_word, NULL);
    pthread_create(&tid_produce_something_plus_small_word, NULL, produce_something_plus_small_word, NULL);
    pthread_create(&tid_produce_small_word_plus_something, NULL, produce_small_word_plus_something, NULL);
    pthread_create(&tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL);

    //tworzenie producentow duzych slow
    pthread_create(&tid_produce_big_word, NULL, produce_big_word, NULL);
    pthread_create(&tid_produce_something_plus_big_word, NULL, produce_something_plus_big_word, NULL);
    pthread_create(&tid_produce_big_word_plus_something, NULL, produce_big_word_plus_something, NULL);
    pthread_create(&tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL);

    //tworzenie producentow slow gdzie pierwsza litera jest duza reszta male
    pthread_create(&tid_produce_first_big_word, NULL, produce_first_big_word, NULL);
    pthread_create(&tid_produce_something_plus_first_big_word, NULL, produce_something_plus_first_big_word, NULL);
    pthread_create(&tid_produce_first_big_word_plus_something, NULL, produce_first_big_word_plus_something, NULL);
    pthread_create(&tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL);

    //tworzenie producentow dwu wyrazowych
    pthread_create(&tid_produce_first_big_word_plus_something_plus_first_big_word, NULL, produce_first_big_word_plus_something_plus_first_big_word, NULL);
    pthread_create(&tid_produce_big_word_plus_something_plus_big_word, NULL, produce_big_word_plus_something_plus_big_word, NULL);
    pthread_create(&tid_produce_small_word_plus_something_plus_small_word, NULL, produce_small_word_plus_something_plus_small_word, NULL);

    //tworzenie 1 konsumenta
    pthread_create(&tid_consume, NULL, consume, NULL);

    while (strcmp(strtok(line, "\n"), "exit") != 0)
    {
        printf("Wpisz 'exit' aby zrezygnowac z tej opcji(nie konczy obecnej pracy) \nlub podaj nowy plik z haslami aby rozpoczac nowa prace\n");
        pom_file = NULL;
        do
        { 
            getline(&line, &len, stdin);
            pom_file = fopen(strtok(line, "\n"), "r");
            if (pom_file == NULL && strcmp(strtok(line, "\n"), "exit"))
            {
                printf("!!! Podano nieprwidlowy plik z haslami %s!!! \n\n", strtok(line, "\n"));
            }
        } while (pom_file == NULL && strcmp(strtok(line, "\n"), "exit") != 0);
        if (strcmp(strtok(line, "\n"), "exit") != 0)
        {
            printf("Zakonczono obecne prace, nastapi rozpoczecie nowej pracy\n");
            //zabijanie producentow malych slow
            pthread_mutex_lock(&shared.mutex);
            pthread_kill(tid_produce_small_word, SIGTERM);
            pthread_kill(tid_produce_something_plus_small_word, SIGTERM);
            pthread_kill(tid_produce_small_word_plus_something, SIGTERM);
            pthread_kill(tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric, SIGTERM);

            //zabijanie producentow duzych slow
            pthread_kill(tid_produce_big_word, SIGTERM);
            pthread_kill(tid_produce_something_plus_big_word, SIGTERM);
            pthread_kill(tid_produce_big_word_plus_something, SIGTERM);
            pthread_kill(tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric, SIGTERM);

            //zabijanie producentow slow gdzie pierwsza litera jest duza reszta male
            pthread_kill(tid_produce_first_big_word, SIGTERM);
            pthread_kill(tid_produce_something_plus_first_big_word, SIGTERM);
            pthread_kill(tid_produce_first_big_word_plus_something, SIGTERM);
            pthread_kill(tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, SIGTERM);

            //zabijanie producentow dwu wyrazowych
            pthread_kill(tid_produce_first_big_word_plus_something_plus_first_big_word, SIGTERM);
            pthread_kill(tid_produce_big_word_plus_something_plus_big_word, SIGTERM);
            pthread_kill(tid_produce_small_word_plus_something_plus_small_word, SIGTERM);

            pthread_kill(tid_consume, SIGTERM);
            // //czekanie na producentow jesli zakonczyli dzialanie
            pthread_mutex_unlock(&shared.mutex);

            pthread_join(tid_produce_small_word, NULL);
            pthread_join(tid_produce_something_plus_small_word, NULL);
            pthread_join(tid_produce_small_word_plus_something, NULL);
            pthread_join(tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL);

            pthread_join(tid_produce_big_word, NULL);
            pthread_join(tid_produce_something_plus_big_word, NULL);
            pthread_join(tid_produce_big_word_plus_something, NULL);
            pthread_join(tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL);

            pthread_join(tid_produce_first_big_word, NULL);
            pthread_join(tid_produce_something_plus_first_big_word, NULL);
            pthread_join(tid_produce_first_big_word_plus_something, NULL);
            pthread_join(tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL);

            //dwu wyrazowi
            pthread_join(tid_produce_first_big_word_plus_something_plus_first_big_word, NULL);
            pthread_join(tid_produce_big_word_plus_something_plus_big_word, NULL);
            pthread_join(tid_produce_small_word_plus_something_plus_small_word, NULL);

            pthread_join(tid_consume, NULL);

            //Zerowanie zmiennych

            for (int i = 0; i < ilosc_hasel; i++)
            {
               free(hasla[i]);

                 if( shared.znalezione[i] == 1||shared.znalezione[i] == 2) //szuakmy ktroe haslo zostalo zlamane, shared.znalezione[a] == 1 oznacza wyprodukowane zlamane haslo
            {
                free(shared.zlamane_hasla[i]);
            }
                free(shared.zlamane_hasla_md5[i]);

            }
            free(hasla);
            free(shared.zlamane_hasla);
            free(shared.zlamane_hasla_md5);
            free(shared.znalezione);
            shared.liczba_zlamanych = 0;
            ilosc_hasel = 0;
            
            //Wczytanie hasel i rozpoczecie pracy
            wczytaj_hasla(strtok(line, "\n"));
             shared.liczba_zlamanych = 0;
            for (int i = 0; i < ilosc_hasel; i++)
            {
                shared.znalezione[i] = 0;
            }

            pthread_create(&tid_produce_small_word, NULL, produce_small_word, NULL);
            pthread_create(&tid_produce_something_plus_small_word, NULL, produce_something_plus_small_word, NULL);
            pthread_create(&tid_produce_small_word_plus_something, NULL, produce_small_word_plus_something, NULL);
            pthread_create(&tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL);

            //tworzenie producentow duzych slow
            pthread_create(&tid_produce_big_word, NULL, produce_big_word, NULL);
            pthread_create(&tid_produce_something_plus_big_word, NULL, produce_something_plus_big_word, NULL);
            pthread_create(&tid_produce_big_word_plus_something, NULL, produce_big_word_plus_something, NULL);
            pthread_create(&tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL);

            //tworzenie producentow slow gdzie pierwsza litera jest duza reszta male
            pthread_create(&tid_produce_first_big_word, NULL, produce_first_big_word, NULL);
            pthread_create(&tid_produce_something_plus_first_big_word, NULL, produce_something_plus_first_big_word, NULL);
            pthread_create(&tid_produce_first_big_word_plus_something, NULL, produce_first_big_word_plus_something, NULL);
            pthread_create(&tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL, produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL);

            //tworzenie producentow dwu wyrazowych
            pthread_create(&tid_produce_first_big_word_plus_something_plus_first_big_word, NULL, produce_first_big_word_plus_something_plus_first_big_word, NULL);
            pthread_create(&tid_produce_big_word_plus_something_plus_big_word, NULL, produce_big_word_plus_something_plus_big_word, NULL);
            pthread_create(&tid_produce_small_word_plus_something_plus_small_word, NULL, produce_small_word_plus_something_plus_small_word, NULL);
            //tworzenie 1 konsumenta
            pthread_create(&tid_consume, NULL, consume, NULL);
        }
        if((strcmp(strtok(line, "\n"), "exit") == 0)){
            printf("Wyjscie z mozliwosci wybierania pliku\n");
        }
    }
    free(line);

    //czekanie na producentow jesli zakonczyli dzialanie
    pthread_join(tid_produce_small_word, NULL);
    pthread_join(tid_produce_something_plus_small_word, NULL);
    pthread_join(tid_produce_small_word_plus_something, NULL);
    pthread_join(tid_produce_something_plus_small_word_plus_something_sides_lenght_symetric, NULL);

    pthread_join(tid_produce_big_word, NULL);
    pthread_join(tid_produce_something_plus_big_word, NULL);
    pthread_join(tid_produce_big_word_plus_something, NULL);
    pthread_join(tid_produce_something_plus_big_word_plus_something_sides_lenght_symetric, NULL);

    pthread_join(tid_produce_first_big_word, NULL);
    pthread_join(tid_produce_something_plus_first_big_word, NULL);
    pthread_join(tid_produce_first_big_word_plus_something, NULL);
    pthread_join(tid_produce_something_plus_first_big_word_plus_something_sides_lenght_symetric, NULL);

    //dwu wyrazowi
    pthread_join(tid_produce_first_big_word_plus_something_plus_first_big_word, NULL);
    pthread_join(tid_produce_big_word_plus_something_plus_big_word, NULL);
    pthread_join(tid_produce_small_word_plus_something_plus_small_word, NULL);

    // zakonczenie watku konsumenta
    pthread_join(tid_consume, NULL);
    // zwolnienie dynamicznej pamieci, w celu unikniecia wyciekow
    for (int i = 0; i < ilosc_hasel; i++)
    {
        free(hasla[i]);
        free(shared.zlamane_hasla[i]);
        free(shared.zlamane_hasla_md5[i]);
    }
    free(hasla);
    free(shared.zlamane_hasla);
    free(shared.zlamane_hasla_md5);
    free(shared.znalezione);

    for (int i = 0; i < lines_slownik; i++)
    {
        free(slownik[i]);
    }
    free(slownik);
    return 0;
}

void consume_wait(int i)
{   FILE *fptr;
    fptr = fopen("scores.txt","a");
    int a = 0;
    
    for (;;)
    {
        pthread_mutex_lock(&shared.mutex);
        if (i < shared.liczba_zlamanych) // jesli liczby odczytanych produktow jest mniejsza od liczby wyprodukowanych
        {
            for (a = 0; a < ilosc_hasel && shared.znalezione[a] != 1; a++) //szuakmy ktroe haslo zostalo zlamane, shared.znalezione[a] == 1 oznacza wyprodukowane zlamane haslo
            {
            }
            if (a == ilosc_hasel)
            {
                a = ilosc_hasel - 1;
            }
            if (shared.znalezione[a] == 1)
            {
                printf("Zlamane haslo nr.%d\nJawna postac hasla:%s\nMD5:%s\n\n", a, shared.zlamane_hasla[a], shared.zlamane_hasla_md5[a]); // wyswietalmych info
                fprintf(fptr,"Zlamane haslo nr.%d\nJawna postac hasla:%s\nMD5:%s\n\n", a, shared.zlamane_hasla[a], shared.zlamane_hasla_md5[a]); //zapisanie informacji w pliku
                shared.znalezione[a] = 2;
            } //onaczamy produkt jako odczytany
            fclose(fptr);
            pthread_mutex_unlock(&shared.mutex);
            
            return; //wyjscie z consume_wait
        }
        pthread_mutex_unlock(&shared.mutex);
    }
    
    
}
void *
consume(void *arg)
{
    int i;
    signal(SIGHUP, reakcja_SIGHUP);
    signal(SIGTERM, reakcja_SIGTERM);
    for (i = 0; i < ilosc_hasel; i++) // dala tylu hasel ile okreslono
    {
        //printf("Konsument oczekuje na hasla \n");
        consume_wait(i); //czakwanie na produkcje jakiego kolwiek zlamanego hasla
    }
    printf("Koniec pracy konsumenta\n");
    return (NULL);
}
