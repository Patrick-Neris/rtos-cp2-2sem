#include <stdio.h> 
#include <stdlib.h> 
#include <stdlib.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h" 

#define NOME "Patrick Costa Neris"
#define RM "88425"

// Definições do sistema
#define TAMANHO_FILA 5
#define TEMPO_ESPERA_RECEPCAO pdMS_TO_TICKS(3000) // 3 segundos
#define TEMPO_SUPERVISAO pdMS_TO_TICKS(5000) // 5 segundos
#define WDT_TIMEOUT_S 10 // 10 segundos (Deve ser o mesmo valor do menuconfig)

// Fila para comunicação entre tarefas
QueueHandle_t xFilaDeDados;

// Flags para o módulo de supervisão
volatile bool flagGeracaoOk = false;
volatile bool flagRecepcaoOk = false;

/**
 * @brief Módulo de Geração de Dados
 */
void vTaskDataGenerator(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    int valor = 0;
    while (1) {
        // CONDIÇÃO DE FALHA MODIFICADA
        if (valor == 10) {
            printf("{%s - RM: %s} [GERACAO] SIMULANDO FALHA... PARANDO ENVIO POR 20S.\n", NOME, RM);
            // Loop para pausar o envio, mas continuar alimentando o WDT
            for (int i = 0; i < 20; i++) {
                ESP_ERROR_CHECK(esp_task_wdt_reset()); // Continua alimentando o WDT para não ser o culpado
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
        if (xQueueSend(xFilaDeDados, &valor, (TickType_t)0) == pdPASS) {
            printf("{%s - RM: %s} [GERACAO] Dado %d enviado com sucesso!\n", NOME, RM, valor);
        } else {
            printf("{%s - RM: %s} [GERACAO] Fila cheia, dado %d descartado.\n", NOME, RM, valor);
        }
        valor++;
        flagGeracaoOk = true;
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Módulo de Recepção de Dados
 */
void vTaskDataReceiver(void *pvParameters) {
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
            flagRecepcaoOk = true;
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
        printf("{%s - RM: %s} [SUPERVISAO] Status: Gerador [%s], Receptor [%s]\n", NOME, RM, flagGeracaoOk ? "ATIVO" : "INATIVO", flagRecepcaoOk ? "ATIVO" : "INATIVO");
        flagGeracaoOk = false;
        flagRecepcaoOk = false;
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
    xTaskCreate(vTaskDataGenerator, "Gerador de Dados", 2048, NULL, 5, NULL);
    xTaskCreate(vTaskDataReceiver, "Receptor de Dados", 2048, NULL, 5, NULL);
    xTaskCreate(vTaskSupervisor, "Supervisor", 3072, NULL, 4, NULL);
    printf("{%s - RM: %s} [SISTEMA] Inicializacao concluida. WDT ja ativo.\n", NOME, RM);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}