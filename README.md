# Carregador 4T 13A — Firmware

**Versão atual: 1.2.0**

## Alterações desta versão (a partir da 1.1.3)

1. Conversão de corrente de float para inteiro (fixed-point).
2. Tempo de sondagem inicial de 5s → 15s por canal (modo operacional).
3. Piso de tolerância a inrush de 500ms → 250ms.
4. Correção do bug de reordenamento (canal com sobrecorrente não sendo demovido corretamente na fila de prioridade).
5. Correção do acesso fora dos limites do array (`IndexTemp < 5` → `< 3`).
