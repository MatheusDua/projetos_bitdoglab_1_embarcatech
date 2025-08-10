//bibliotecas para utilizar o GPIO, tempo e controlar o PWM
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

//sinaliza em qual pino fica cada componente e a frequência do buzzer
#define led_RG_pin 1 //Sinal vermelho e de pedestre(mesmo pino devido a funcionarem semelhantemente)
#define led_y_pin 2
#define led_g_pin 3
#define BT 5
#define BuzzerPin 6
#define buzzer_hz 2500


void pinagem(){
//inicia os pinos

  gpio_init(led_RG_pin);
  gpio_init(led_y_pin);
  gpio_init(led_g_pin);
  gpio_init(BT);


  //defini entradas e saídas
  gpio_set_dir(led_RG_pin, GPIO_OUT);
  gpio_set_dir(led_y_pin, GPIO_OUT);
  gpio_set_dir(led_g_pin, GPIO_OUT);
  gpio_set_dir(BT, GPIO_IN);
  //defini o botão como sendo de pull-up  
  gpio_pull_up(BT);

  //defini o buzzer como saída
  gpio_init(BuzzerPin);
  gpio_set_dir(BuzzerPin, GPIO_OUT);

}

void PWM_buzzer(uint pin){
  
  gpio_set_function(pin, GPIO_FUNC_PWM);   //defini um pino PWM

  uint slice_num = pwm_gpio_to_slice_num(pin); //pega o slice do PWM a um pino
 
  //configuração da frequência para o buzzer 
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (buzzer_hz * 1000));
  pwm_init (slice_num, &config, true);

  //garante que o pino começará em nível baixo
  pwm_set_gpio_level(pin, 0);


}

void autenticarbotao(){
  //verifica se o botão foi pressionado 
 if (gpio_get(BT) == 0 &&  gpio_get(led_RG_pin) != 1)  {
    //ligar o LED amarelo e desligar o verde    
    gpio_put(led_y_pin, 1);
    gpio_put(led_g_pin, 0);
    sleep_ms(5000);
    //acender o farol de pedestre e acionar o som do buzzer
    gpio_put(led_RG_pin, 1);
    gpio_put(led_y_pin, 0);
    pwm_set_gpio_level(BuzzerPin, 1);
    sleep_ms(15000);
    //finaliza a sequência do semáforo desligando o buzzer e ligando o sinal verde
    gpio_put(led_RG_pin, 0);
    gpio_put(led_y_pin, 0);
    gpio_put(led_g_pin, 1);
    pwm_set_gpio_level(BuzzerPin, 0);

  }
  //caso o sinal já esteja no vermelho, liga o buzzer direto e continua a sequência
 else if (gpio_get(BT) == 0 &&  gpio_get(led_RG_pin) == 1 )  {
  gpio_put(led_RG_pin, 1);
  gpio_put(led_y_pin, 0);
  pwm_set_gpio_level(BuzzerPin, 1);
  sleep_ms(15000);

  gpio_put(led_RG_pin, 0);
  gpio_put(led_y_pin, 0);
  gpio_put(led_g_pin, 1);
  pwm_set_gpio_level(BuzzerPin, 0);

 }
}
int main(){

  pinagem(); //dar início ao pinagem na parte principal
  PWM_buzzer(BuzzerPin); //atribuir ao buzzer o pino PWM
 
  //criar um loop para manter o farol aceso 
  while (true){
    //inicia o semaforo com o sinal verde acesso
    gpio_put(led_RG_pin, 0);
    gpio_put(led_y_pin, 0);
    gpio_put(led_g_pin, 1);
   
    //O código verificará a cada milissegundo se o botão foi pressionado
    //caso não seja pressionado, passa o tempo e o semáforo continua
    for(int i=0; i<=10000;i++){
      autenticarbotao();
      sleep_ms(1);

  }
    //acender o sinal amarelo e verificar se o botão foi pressionado  
    gpio_put(led_RG_pin, 0);
    gpio_put(led_y_pin, 1);
    gpio_put(led_g_pin, 0);
    for(int i=0; i<=3000;i++){
      autenticarbotao();
      sleep_ms(1);
  }
    //acender o sinal vermelho e o de pedestre e verificar se o botão foi pressionado
    gpio_put(led_RG_pin, 1);
    gpio_put(led_y_pin, 0);
    gpio_put(led_g_pin, 0);
    for(int i=0; i<=8000;i++){
      autenticarbotao();
      sleep_ms(1);
    }
  //ao encerrar o ciclo, voltar para a condição while formando um loop    
   }
}
  