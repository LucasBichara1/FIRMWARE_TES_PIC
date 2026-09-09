//Versão 1.2.0 para a OP2107, placa 04007012_B.
//Placa com CI de 13A e firmware com corte para 10A.


#include <xc.h>
#include <pic16f1826.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// #include "Display_Oled.h"
#define _XTAL_FREQ 8000000

// CONFIG1
#pragma config FOSC = INTOSC  // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF     // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF    // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON     // MCLR Pin Function Select (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF       // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF      // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON     // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF     // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF    // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config WRT = OFF   // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF // PLL Disable (4x PLL disabled)
#pragma config STVREN = ON // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO   // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = ON    // Low-Voltage Programming Enable (Low-voltage programming enabled)

// Entradas
#define SW PORTBbits.RB7            // Jumper na barra de programacao para entrar em modo teste
#define CURRENT_SENSE PORTAbits.RA0 // Sinal de corrente
#define SEL_1 PORTAbits.RA1         //
#define SEL_2 PORTAbits.RA2
#define SEL_3 PORTAbits.RA3
#define SEL_4 PORTAbits.RA4

// Saidas
#define RELE_1 LATBbits.LATB1 // Rele 1
#define RELE_2 LATBbits.LATB2 // Rele 2
#define RELE_3 LATBbits.LATB3 // Rele 3
#define RELE_4 LATBbits.LATB4 // Rele 4
// #define RELE_5 LATAbits.LATA6 // Rele 5
// #define RELE_6 LATAbits.LATA7 // Rele 6

uint16_t CURRENT_SET = 100; // Define o limite da corrente de corte (10A -> 10*10 = 100)

#define CH_AN0 0b00000000 // Canal analogico do sensor de corrente AN0 (Register ADCON0)
#define ON 1
#define OFF 0
// Não P
// int tempo_carga; // tempo em que os relés ficam acionados carregando o notebook (min)

uint8_t RELE_NUM;
char display[22] = "<///<///<///<///<///<";
uint8_t _millis;
uint16_t _millis2;
uint16_t base_45;
uint16_t base_s;
uint8_t base_tensao;
uint8_t Ativou_Rele = 0;

uint8_t VALOR_CORRENTE[4];
uint8_t ORDENA_CORRENTE[4] = {0, 1, 2, 3};
uint8_t FALHOU_CORRENTE[4] = {0, 0, 0, 0};
uint8_t temp_rele_num = 0;
uint8_t IndexTemp;

char versao[8] = "V.1.2.0";

uint16_t timer;

uint8_t timer_text[][17] = {
    "TIMER:/DESLIGADO",
    "TIMER:/1/H",
    "TIMER:/2/H",
    "TIMER:/3/H"};

uint16_t ADC_READ(uint8_t ch);

enum Maquina_Estado
{
    INICIA_PROCESSO,
    AGUARDA_LEITURA,
    ATIVA_RELE,
    DESATIVA_RELE,
    LE_CORRENTE,
    DESLIGA,
};

uint8_t ESTADO;

void setup()
{
    OSCCONbits.IRCF = 0xE;
    OSCCONbits.SCS = 0x3;

    // Configuracao PORT A
    TRISA = 0b00001111;  // PORTs A0,A1,A2, A3 como entrada, os demais como saidas
    ANSELA = 0b00000001; // PORTA0 como entrada analogica (AN0)
    PORTA = 0x00;

    // Configuracao PORT B
    TRISB = 0b10000000; // PORTs B7 como entrada, os demais como saidas
    ANSELB = 0x00;
    PORTB = 0x00;

    ADCON1 = 0b11100000;     // Right Justified, FOSC/64
    OPTION_REG = 0b00000010; // Pull-ups are enabled, Pre scaler 1:8
    WPUB = 0b10000000;       // Pull-up RB7 enable

    INTCON = 0b11100000; // Controle de interrupcoes
}

// Realiza a leitura do canal ADC passado como parametro
uint16_t ADC_READ(uint8_t ch) {
    uint16_t result = 0;
    uint8_t i = 0;
    uint16_t ADC_Value;

    ADCON0bits.CHS = ch; //Configura canal analogico
    ADCON0bits.ADON = 1;
    ADRESH = 0; /*Flush ADC output Register*/
    ADRESL = 0;

    ADCON0bits.GO = 1; // Habilita ADC e inicia conversao
    while(ADCON0bits.GO_nDONE == 1); // Aguarda o fim da conversao Go/done = 0 
    ADC_Value = (ADRESH*256) | (ADRESL); // Combina 8-bit LSB e 2-bit MSB

    return ADC_Value;
}

uint16_t CORRENTE(){
    uint32_t soma_quadrados = 0;
    int8_t temp;

    // Realiza 500 leituras e faz a somatoria do valor ao quadrado nesse tempo (pega aproximadamente 10 ciclos da senoide)
    // I(A) = ADC*(Vref/1023)/66mV_por_A - (Vref/2)/66mV_por_A, ACS712-30A, Vref calibrado ~4.996V
    // (coeficientes 73982 e 37878788 = 0.073982 e 37.878788 escalados por 1.000.000, sem uso de float)
    for(int i = 0; i < 500; i++)
    {
        temp = (int8_t)(((int32_t)ADC_READ(CH_AN0) * 73982L - 37878788L) / 1000000L);
        soma_quadrados += (uint32_t)((int16_t)temp * (int16_t)temp); // Somatoria da corrente ao quadrado
    }

    return (uint16_t)(soma_quadrados / 500); // Media (unidade: corrente ao quadrado, em A^2)
}

void set_rele(uint8_t rel, uint8_t set)
{
    const char status[2] = {'<', '>'};
    switch (rel)
    {
    case 0:
        RELE_1 = set;
        break;

    case 1:
        RELE_2 = set;
        break;

    case 2:
        RELE_3 = set;
        break;

    case 3:
        RELE_4 = set;
        break;

    default:
        break;
    }
}

int current_get()
{
    for (uint8_t i = 0; i < 4; i++)
    {
        set_rele(i, 0);
        ORDENA_CORRENTE[i] = i;
        FALHOU_CORRENTE[i] = 0;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        set_rele(i, 1);
        if (SW == 0) // COM JUMPER NO GND E PGD, MODO TESTE
        {
            _millis2 = 2000; // 2 SEGUNDOS PARA CADA ACIONAMENTO DOS RELES
        }
        else // SEM JUMPER NO GND E PGD, MODO OPERACIONAL
        {
            _millis2 = 15000; // 15 SEGUNDOS PARA CADA ACIONAMENTO DOS RELES
        }

        while (_millis2 > 0)
        {
            VALOR_CORRENTE[i] = CORRENTE();

            if (VALOR_CORRENTE[i] >= CURRENT_SET)
            {
                if (_millis2 < 14750) // tolerancia de 250ms de inrush antes de permitir o corte
                {
                    _millis2 = 0;
                }
            }

        }
        set_rele(i, 0);

        if (VALOR_CORRENTE[i] >= CURRENT_SET)
        { // caso a corrente do banco seja maior que o limite, joga o banco para o último da fila.
            VALOR_CORRENTE[i] = 0;
            FALHOU_CORRENTE[i] = 1;
        }
    }
}

int current_sort()
{
    uint8_t i;
    uint8_t maxIndex = 0;

    for (i = 0; i < (4 - 1); i++)
    {
        maxIndex = i;
        for (uint8_t k = i + 1; k > 0; k--)
        {
            uint8_t a = ORDENA_CORRENTE[k];
            uint8_t b = ORDENA_CORRENTE[k - 1];
            uint8_t deve_trocar;

            if (FALHOU_CORRENTE[a] != FALHOU_CORRENTE[b])
            {
                deve_trocar = FALHOU_CORRENTE[b]; // quem falhou na sondagem vai sempre para o final, mesmo com valor empatado
            }
            else
            {
                deve_trocar = (VALOR_CORRENTE[a] > VALOR_CORRENTE[b]);
            }

            if (deve_trocar)
            {
                ORDENA_CORRENTE[k] = b;
                ORDENA_CORRENTE[k - 1] = a;
            }
        }
    }
}

void main()
{
    setup();
    base_s = 1000;
    uint8_t config_timer = 0;
    int countBut = 0;
    // unsigned int percentual;

    if (SW == 1)
    {
        timer = 5;
        uint16_t current_init = CORRENTE();
    }
    else
    {
    }

    ESTADO = INICIA_PROCESSO;

    while (1)

    {
        uint16_t current = CORRENTE();

        if (current >= CURRENT_SET)
        {
            ESTADO = DESATIVA_RELE;
        }

        switch (ESTADO)
        {
        case INICIA_PROCESSO:

            // COOLER = ON; // COOLER ACIONADO
            current_get();
            current_sort();
            _millis = 5;
            base_45 = 0;
            RELE_NUM = 0;
            ESTADO = ATIVA_RELE;
            base_tensao = 0;
            break;

        case ATIVA_RELE:

            RELE_NUM++;
            set_rele(ORDENA_CORRENTE[RELE_NUM - 1], 1);
            Ativou_Rele = 1;

            _millis2 = 150;
            while (_millis2 > 0)
            {
            }
            ESTADO = AGUARDA_LEITURA;
            break;

        case AGUARDA_LEITURA:
            // Conta base de tempo para leitura de corrente (5s)
            if (_millis == 0)
            {

                if (SW == 0) // COM JUMPER NO GND E PGD, MODO TESTE
                {
                    _millis = 2; // 2 SEGUNDOS PARA CADA ACIONAMENTO DOS RELES
                }
                else // SEM JUMPER NO GND E PGD, MODO OPERACIONAL
                {
                    _millis = 5; // 5 SEGUNDOS PARA CADA ACIONAMENTO DOS RELES
                }

                if ((RELE_NUM < 4))
                {
                    ESTADO = ATIVA_RELE;
                }
                else
                {
                    Ativou_Rele = 0;
                }
                //}
            }

            if (base_45 >= 2700)
            {
                if (RELE_NUM < 4)
                {
                    ESTADO = INICIA_PROCESSO;
                    base_45 = 0;
                }
                else
                {
                    base_45 = 2700;
                }
            }

            if (base_tensao == 0)
            {
                base_tensao = 10;
                // convert_percentual(percentual);
                // text(display_percentual, 1, 40);
            }

            break;

        case DESATIVA_RELE:
            set_rele(ORDENA_CORRENTE[RELE_NUM - 1], 0);
            IndexTemp = RELE_NUM - 1;

            RELE_NUM--;

            if (RELE_NUM == 0)
            {
                ESTADO = INICIA_PROCESSO;
            }
            else
            {
                if (Ativou_Rele == 1)
                {
                    Ativou_Rele = 0;
                    if (IndexTemp < 3) // array ORDENA_CORRENTE tem apenas 4 posicoes (0-3); IndexTemp+1 nao pode passar de 3
                    {
                        temp_rele_num = ORDENA_CORRENTE[IndexTemp];
                        ORDENA_CORRENTE[IndexTemp] = ORDENA_CORRENTE[IndexTemp + 1];
                        ORDENA_CORRENTE[IndexTemp + 1] = temp_rele_num;
                    }
                    else
                    {
                    }
                }
                else
                {
                }

                if (SW == 0) // COM JUMPER NO GND E PGD, MODO TESTE
                {
                    _millis = 5;
                }
                else // SEM JUMPER NO GND E PGD, MODO OPERACIONAL
                {
                    _millis = 120;
                }
                ESTADO = AGUARDA_LEITURA;
            }
            break;
        }
    }
}

// Tratamento de interrupcoes
void __interrupt() interrupcoes(void)
{
    // Timer 0, base de tempo 1 ms
    if ((T0IF))
    {
        T0IF = 0;
        TMR0 = 6;
        if (_millis2 != 0)
        {
            _millis2--;
        }

        if (--base_s == 0)
        {
            base_s = 1000;
            _millis--;
            base_45++;

            if (base_tensao != 0)
            {
                base_tensao--;
            }

            if (timer != 0)
            {
                timer--;
            }
        }
    }
}

