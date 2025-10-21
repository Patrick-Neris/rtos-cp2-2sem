#include <stdio.h> 
#include <stdlib.h> 
#include <stdlib.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h" 

#define NOME "Patrick Costa Neris"
#define RM "88425"

#define TAMANHO_FILA 5
#define TEMPO_ESPERA_RECEPCAO pdMS_TO_TICKS(3000)
#define TEMPO_SUPERVISAO pdMS_TO_TICKS(5000)
#define WDT_TIMEOUT_S 10

QueueHandle_t xFilaDeDados;

volatile bool flagGeracao = false;
volatile bool flagRecepcao = false;

/**
 * @brief Módulo de Geração de Dados
 */
void vTaskGeradorDados(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int valor = 0;
    while (1) {
        if (valor == 10) {
            printf("{%s - RM: %s} [GERACAO] FALHA DETECTADA... PARANDO ENVIO POR 20S.\n", NOME, RM);
            for (int i = 0; i < 20; i++) {
                ESP_ERROR_CHECK(esp_task_wdt_reset()); // Para avisar ao WDT que não é o culpado.
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        if (xQueueSend(xFilaDeDados, &valor, (TickType_t)0) == pdPASS) {
            printf("{%s - RM: %s} [GERACAO] Dado %d enviado com sucesso!\n", NOME, RM, valor);
        } else {
            printf("{%s - RM: %s} [GERACAO] Fila cheia, dado %d descartado.\n", NOME, RM, valor);
        }
        valor++;
        flagGeracao = true;
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Módulo de Recepção de Dados
 */
void vTaskReceptorDados(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int *valorRecebidoPtr;
    int falhasTimeout = 0;
    while (1) {
        int valorBuffer;
        if (xQueueReceive(xFilaDeDados, &valorBuffer, TEMPO_ESPERA_RECEPCAO) == pdPASS) {
            falhasTimeout = 0;
            valorRecebidoPtr = (int *)malloc(sizeof(int));
            if (valorRecebidoPtr != NULL) {
                *valorRecebidoPtr = valorBuffer;
                printf("{%s - RM: %s} [RECEPCAO] Dado %d recebido e transmitido.\n", NOME, RM, *valorRecebidoPtr);
                free(valorRecebidoPtr);
            } else {
                printf("{%s - RM: %s} [RECEPCAO] Erro ao alocar memoria!\n", NOME, RM);
            }
            flagRecepcao = true;
        } else {
            falhasTimeout++;
            switch (falhasTimeout) {
                case 1:
                    printf("{%s - RM: %s} [AVISO] Timeout ao receber dados (1/3).\n", NOME, RM);
                    break;
                case 2:
                    printf("{%s - RM: %s} [RECUPERACAO] Timeout novamente, tentando recuperar... (2/3).\n", NOME, RM);
                    break;
                default:
                    printf("{%s - RM: %s} [ERRO CRITICO] Falha grave no recebimento. O sistema sera reiniciado pelo WDT. (3/3)\n", NOME, RM);
                    while(1);
            }
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Módulo de Supervisão
 */
void vTaskSupervisor(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while (1) {
        vTaskDelay(TEMPO_SUPERVISAO);
        printf("{%s - RM: %s} [SUPERVISAO] Status: Gerador [%s], Receptor [%s]\n", NOME, RM, flagGeracao ? "ATIVO" : "INATIVO", flagRecepcao ? "ATIVO" : "INATIVO");
        flagGeracao = false;
        flagRecepcao = false;
        ESP_ERROR_CHECK(esp_task_wdt_reset());
    }
}

void app_main(void) {
    printf("{%s - RM: %s} [SISTEMA] Iniciando CP2 - Sistema de Dados Robusto.\n", NOME, RM);
    xFilaDeDados = xQueueCreate(TAMANHO_FILA, sizeof(int));
    if (xFilaDeDados == NULL) {
        printf("{%s - RM: %s} [SISTEMA] Falha ao criar a fila.\n");
        return;
    }
    xTaskCreate(vTaskGeradorDados, "Gerador de Dados", 2048, NULL, 5, NULL);
    xTaskCreate(vTaskReceptorDados, "Receptor de Dados", 2048, NULL, 5, NULL);
    xTaskCreate(vTaskSupervisor, "Supervisor", 3072, NULL, 4, NULL);
    printf("{%s - RM: %s} [SISTEMA] Inicializacao concluida. WDT ja ativo.\n", NOME, RM);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}