#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

//Pinos de acordo com o BitDogLab
#define PIN_R 13
#define PIN_G 11
#define PIN_B 12
#define BTN_A_PIN 5
const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

//Garante que o valor de cor e do botão estejam inicialmente zerados
int cor = 0;
int A_state = 0; 


void iniciarpin(){ //void para iniciar a comunicação dos pinos com o display

//Configuração dos pinos I2C
    i2c_init(i2c1, ssd1306_i2c_clock * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);


    ssd1306_init();


  //Renderização do display
    struct render_area frame_area = {
        start_column : 0,
        end_column : ssd1306_width - 1,
        start_page : 0,
        end_page : ssd1306_n_pages - 1
    };

    calculate_render_area_buffer_length(&frame_area);

  //Zera o valor do display
    uint8_t ssd[ssd1306_buffer_length];
    memset(ssd, 0, ssd1306_buffer_length);
    render_on_display(ssd, &frame_area);

    restart:
    

    //Cria ocasiões para cada estado do sinal
    char *textos[2]; 

    if (cor == 1) {
        textos[0] = "  SINAL ABERTO ";
        textos[1] = "LIBERADO PASSAR";
    } else if (cor == 2) {
        textos[0] = " SINAL AMARELO ";
        textos[1] = "    AGUARDE ";
    } else if (cor == 3) {
        textos[0] = " SINAL FECHADO ";
        textos[1] = " NAO ATRAVESSE";
    }
 
//Escreve o local do texto e exibe ele no display
  int y = 16;
  for (uint i = 0; i < count_of(textos); i++) {
    ssd1306_draw_string(ssd, 5, y, textos[i]);
    y += 32;
  }
  render_on_display(ssd, &frame_area);

}

void SinalAberto(){ 

    gpio_put(PIN_R, 0);
    gpio_put(PIN_G, 1);
    gpio_put(PIN_B, 0);   
    cor = 1;
    iniciarpin();

}

void SinalAtencao(){

    gpio_put(PIN_R, 1);
    gpio_put(PIN_G, 0);
    gpio_put(PIN_B, 0);
    cor = 2;
    iniciarpin();

}

void SinalFechado(){

    gpio_put(PIN_R, 1);
    gpio_put(PIN_G, 0);
    gpio_put(PIN_B, 0);
    cor = 3;
    iniciarpin();

}

//Estado de espera para ativação do botão
int WaitWithRead(int timeMS){
    for(int i = 0; i < timeMS; i = i+100){
        A_state = !gpio_get(BTN_A_PIN);
        if(A_state == 1){
            return 1;
        }
        sleep_ms(100);
    }
    return 0;
}
int main(){
    
    //INICIAÇÃO DOS LED E BOTÃO
    gpio_init(PIN_R);
    gpio_set_dir(PIN_R, GPIO_OUT);
    gpio_init(PIN_G);
    gpio_set_dir(PIN_G, GPIO_OUT);
    gpio_init(PIN_B);
    gpio_set_dir(PIN_B, GPIO_OUT);

    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);
    
    //Loop do sinal de pedestre
    while(true){
    
        SinalFechado();
        
        A_state = WaitWithRead(5000);  //Estado de observação do valor do botão e tempo de permanência do sinal fechado
        


        if(A_state){  //Botão apertado inicia o ciclo para o pedestre passar
            
            SinalAtencao();
            sleep_ms(3000);

            SinalAberto();
            sleep_ms(10000);

        }else{  //Botão não apertado continuar o ciclo do sinal                      
                                      
            SinalAtencao();
            sleep_ms(4000);

            SinalAberto();
            sleep_ms(8000);
        }
                
    }

    return 0;

}