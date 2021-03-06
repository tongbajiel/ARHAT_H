/**
 * Time State Control Simple Library. Prefix for this: TSC_ (tsc_)
 *
 * Библиотека создания простых конечных автоматов с задержками исполнения по времени
 * в миллисекундах через функцию millis() или через getOvfCount() или через какую ещё.
 *
 * tsc_getTime() -- макрос, который можно переопределить для замены функции получения тиков времени.
 *
 * TSC_Time определен как uint16_t -- 16-битное целое, соответственно максимальная задержка = 65.535сек!
 *
 * Длительность интервалов переопределена для сокращения размера скетчей когда не требуется создавать задержки
 * длительностью более 65.5 секунд. Если надо больше - переопределите этот тип данных.
 *
 * @author Arhat109 at 2015-07-18
 * @see Arhat.lib::examples/TSC_Blink/TSC_Blink.ino
 */
#ifndef _TSC_H_
#define _TSC_H_ 1

#include "arhat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define tscGetTime()   ((TSC_Time)getOvfCount())
//#define tsc_getTime()   ((TSC_Time)millis())

typedef uint16_t TSC_Time;              // переопределение интервалов до 65535 мсек
//typedef uint32_t TSC_Time;            // переопределение интервалов до 49.7 дней.

/**
 * Определение типа "команда КА" -- функция, получающая указатель на свой блок данных КА (TSC_Simple,TSC_Control,..)
 * тут как void* для того, чтобы можно было использовать с разными типами КА. Внутри реализации следует приводить к
 * нужному типу явно, например: (TSC_Simple *)ptr
 */
typedef void (*TSC_Command)(void *);

// ****************** Базовый автомат ***************** //
/**
 * Использует 1 функцию и хранит начало интервала, команду и интервал ожидания
 *
 * Позволяет создавать автономные КА с прямым управлением через запись в структуру данных.
 * Если command == 0, то ничего не вызывает. Можно использовать для однократного или периодического запуска.
 * @example TSC_SimpleHeart_RGB.ino
 */
typedef struct {
  TSC_Time        started_at;           // момент начала текущего интервала ожидания
  TSC_Time        timeout;              // интервал ожидания исполнения команды
  TSC_Command     command;              // команда ожидающая исполнения
} TSC_Simple;

/**
 * функция шага базового автомата для loop(): по завершению интервала вызывает команду из TSC_Simple::command,
 * если она есть. Иначе ничего не делает. Если command не зануляется при исполнении (повторность действий), то
 * время выполнения команды ВХОДИТ в следующий интервал ожидания: сначала фиксирует момент времени и потом
 * вызывает исполнение команды.
 */
void tsc_run( TSC_Simple *_tsc );
void tsc_simple( TSC_Simple *_tsc, TSC_Command command, TSC_Time timeout);

// ****************** Автоматы, управляемые таблицей переходов ***************** //
// @TODO: перевести в С++ класс.
// @TODO: отсюда можно отнаследовать автоматы, возвращающие условия и имеющие более 1 ветки переходов
//        в таблице: бинарные автоматы, автоматы с троичной логикой и т.д.
// @TODO: отсюда же можно отнаследовать автоматы "с памятью" и "стеком исполнения"...

typedef uint16_t TSC_Step_Count;                // размерность нумерации состояний: 16 бит. (FLASH организована по 16бит!)
//typedef uint8_t TSC_Step_Count;                 // размерность нумерации состояний: 8 бит.

/**
 * Определение типа ячейки хранения состояний автомата во FLASH-памяти программ:
 * 1. сначала вызывается команда, а потом(!) выдерживается указанная в строке пауза до следующей
 * 2. переход на следующее состояние next.
 * Все явные изменения состояний (проверки условий) могут делаться извне через tsc_next()
 * Нумерация состояний с нуля.
 *
 * Если таблица не задана - ничего не выполняет (отключение автомата)
 * Если в таблице нет команды - производит задержку до следующего выполнения, ничего не вызывает
 * Время исполнения текущей команды входит в интервал ожидания следующей.
 */
typedef struct PROGMEM {
  TSC_Command           command;                // команда (метод) для исполнения действий состояния
  TSC_Time              timeout;                // временной интервал до(!) следующего состояния (мсек)
  TSC_Step_Count        next;                   // номер следующей команды в таблице состояний КА
} TSC_Step;

// вводим определение типа для хранения текущего состояния конечного автомата (КА) в SRAM:
typedef struct {
  TSC_Time                 started_at;          // момент начала текущего интервала ожидания
  TSC_Time                 timeout;             // интервал до запуска команды по индексу state
  TSC_Step_Count           state;               // номер состояния КА, ожидающий исполнения
  const TSC_Step * PROGMEM table;               // таблица состояний и переходов этого КА
} TSC_Control;

void tsc_init( TSC_Control *_tsc, const TSC_Step * PROGMEM _table, TSC_Step_Count _state );
void tsc_next( TSC_Control *_tsc, TSC_Step_Count _state );
void tsc_step( TSC_Control *_tsc );
void tsc_micro_next( TSC_Control *_tsc, TSC_Step_Count _state );
void tsc_micro_step ( TSC_Control *_tsc );      // шаг цикла КА в микросекундах по 4мксек [0,4,..1024мксек]

void empty(void *_tsc);                         // пропуск действия

// ================================================================================================================= //
// Определения и реализация конечных автоматов для аналоговой оцифровки сигналов через прерывания от ADC             //
// ================================================================================================================= //
#ifdef MAX_ADC

#ifndef _ADC_H_
#define _ADC_H_ 1

// @TODO: требует окончательного тестирования процедуры обработчика прерываний от АЦП
// для массива потоковой оцифровки через прерывания.
typedef struct {
  uint8_t    pin;
  uint8_t    isReady;
  uint16_t   value;
} ADCvalue;

volatile uint8_t    adcCurrent = 0;
ADCvalue            adcVals[MAX_ADC];

/**
 * Interrupt analog read for array analog pins
 * read current analog value and start reading next values from
 */
ISR(ADC_vect)
{
  ADCvalue * curPtr = &adcVals[adcCurrent];

  curPtr->value = (ADCH<<8) + ADCL;
  curPtr->isReady = 1;

  if( ++adcCurrent > MAX_ADC ) { adcCurrent=0; }        // if next is end, goto start

  curPtr = &adcVals[adcCurrent];                        // get next nanalog to read
  curPtr->isReady = 0;
  ADMUX  |= curPtr->pin & 7;
  ADCSRB |= (curPtr->pin & 8) * 4;
  ADCSRA |= _BV(ADSC);                                  // start next read for next pin
}

#endif // _ADC_H_
#endif // MAX_ADC

#ifdef __cplusplus
}
#endif

#endif // _TSC_H_