/**
 * Обработка данных с датчиков цвета TCS3200(TCS3210) в стиле конечных автоматов (без потерь времени на ожидания).
 *
 * Предположено:
 * 0. контроллер - Arduino Mega 2560
 * 1. Замер данных методом подсчета периодов частоты внешним входом счетчика Т5 Atmega2560
 * 2. Максимально - 2 датчика, различаемых сигналом !OE (проверять что на плате датчика он не закорочен на землю принудительно!)
 * 3. Сигналы датчиков S0, S2 -- подключены к +5в (лог.1) и задают максимальную скорость счета и предел частоты <=600кгц.
 * 4. Сигналы Out,S2,S3 -- общие от обоих датчиков. Активность датчиков устанавливается сигналом !OE
 *
 * Для подключения большего числа датчиков к счетчику надо (в этом файле!):
 * а) задать доп. номера пинов для сигналов OE доп. датчиков. Каждый такой сигнал должен приходить на свой пин Ардуины;
 * б) увеличить константу количества датчиков TCS_MAX_SENSORS до требуемого количества;
 *
 * Для использования другого 16-и разрядного таймера (1,3 или 4), если у вас есть его "счетный вход" надо переназначить
 * все ноги на новый таймер и указать его номер .. см. первый блок #define
 *
 * Подключение файла:
 * 1. Убедитесь, что ваш датчик(и) соединены с Мегой как тут предположено. Предварительно проверьте, что датчик(и)
 *    управляются сигналом ОЕ .. а то мои оказались закорочены - чуть не спалил;
 *
 * 2. void setup(){
 *      // ... настройки, те что надо вашей программе ..
 *      tcsSetup(); // настраиваем ноги датчика(ов), таймер и пр. Можно поместить и в начало ..
 *    }
 *
 * 3. void loop()
 *    {
 *      tcsPrepare({0|1|num}); -- вызов настройки заданного датчика на замеры (номер: число, переменная - все равно)
 *
 *      // ... ваш код основной программы ...
 *      // ... в нем можно пользоваться данными из глобального массива tcsColors[num][color]
 *      // ... поскольку в нем лежат только реальные данные.
 *      // ... узнать что данные обновлены можно, например посмотрев счетчик замеров tcsCount (а надо ли?!?)
 *      // ... режимы замеров можно изменять на лету в tcsModes
 *      // ... повторный запуск того же самого датчика можно делать макросом tcsRestart() или просто присваиванием (как в нем)
 *    }
 *
 * !!! Не желательно ускорять замеры шустрее 4мсек, если режим замера предполагает автоподстройку скорости самим датчиком.
 *
 * Остальное - читайте этот файл, пользуйтесь.
 *
 * @author Arhat109, e-mail: arhat109@mail.ru, phone +7-(951)-388-2793
 *
 * Лицензия: полностью бесплатное ПО, в том числе и от любых претензий. Вопросы и пожелания - принимаются на почту.
 * Вы можете его использовать на свой страх и риск любым известным вам способом.
 *
 * Если вам понравился этот код, не возбраняется положить денег на телефон "сколько не жалко".
 */
#ifndef _TCS3200_
#define _TCS3200_

#include "arhat.h"

#ifdef __cplusplus
extern "C" {
#endif
// ====================== Настройка и чтение состояния датчиков ЦВЕТА TCS3200 ========================= //

#if defined(_ARHAT_PINS2560_)
    #define tcsOut    47                    // T5in   => выход обоих датчиков out
    #define tcsICP    48                    // T5icp на будущее: подсчет периода приходящей частоты
    #define tcsS2     44                    // T5outC => управление цветом обоих датчиков
    #define tcsS3     46                    // T5outA => управление цветом обоих датчиков
    #define tcsOE1    49                    // T4ICP  => !OE for TCS3200 1-й датчик
    #define tcsOE2    45                    // T5outB => !OE for TCS3200 2-й датчик
    #define tcsTimer   5
#elif defined(_ARHAT_PINS328P_)
    #define tcsOut     5                    // T1in   => выход обоих датчиков out
    #define tcsICP     8                    // T1icp на будущее: подсчет периода приходящей частоты
    #define tcsS2      3                    //
    #define tcsS3      4                    //
    #define tcsOE1     6                    //
    #define tcsOE2     7                    //
    #define tcsTimer   1
#else
    #error *** ERROR! Not MEGA2560 and NOT UNO or such .. new board? ***
#endif

#define TCS_MAX_SENSORS    2            // Всего датчиков на этом счетчике
#define TCS_MAX_MEASURES   1            // [1,2,4,8,..] для автозамены деления сдвигами
#define TCS_WAIT           2            // мсек на разовый подсчет частоты. Итого fmin=250hz (4лк), замер 4 цветов = 62.5Гц
//#define TCS_WAIT         8              // мсек на разовый подсчет частоты. Итого fmin=125hz (1.5лк), замер 4 цветов = 31.25Гц
//#define TCS_WAIT        16              // мсек на разовый подсчет частоты. Итого fmin=62.5hz (0.9лк), замер 4 цветов = 15.6Гц
//#define TCS_WAIT        32              // мсек на разовый подсчет частоты. Итого fmin=31.25hz (0.4лк), замер 4 цветов = 7.8Гц

#define TCS_MAX_WAIT    4*TCS_WAIT      // время, к которому приводим подсчитанные величины для повышения качества в темноте
#define TCS_CONTRAST     8              // делитель для повышения контраста цвета (1/2,1/4,1/8)

#define TCS_WHITE    0                  // порядок цветов в рез. массиве (значения для tcsColor)
#define TCS_GREEN    1
#define TCS_RED      2
#define TCS_BLUE     3
#define TCS_NOCOLOR  4                  // продолжение: состояние КА "замер завершен"
#define TCS_START    5                  // .. команда конечному автомату "начать замер"

/**
 * Макрос повторного запуска того же датчика:
 */
#define tcsRestart() {tcsColor = TCS_START;}

/**
 * Макрос переноса из временных значений в окончательные без обработки
 * @global tcsTemp[],tcsColors[]
 */
#define tcsRaw()                         \
{                                        \
  tcsColors[tcsCurrent][0] = tcsTemp[0]; \
  tcsColors[tcsCurrent][1] = tcsTemp[1]; \
  tcsColors[tcsCurrent][2] = tcsTemp[2]; \
  tcsColors[tcsCurrent][3] = tcsTemp[3]; \
}

/**
 * Макрос простого усреднения накопленных данных
 * @global tcsTemp[]
 */
#define tcsAvg()                      \
{                                     \
  uint8_t i=4;                        \
  do{                                 \
    tcsTemp[--i] /= TCS_MAX_MEASURES; \
  }while( i>0 );                      \
}

/**
 * Макрос перевода яркостного канала из частоты в люксы
 * @global tcsTemp[0]
 */
#define tcsLukses()         \
{                           \
  int tg = tcsTemp[0];      \
  tcsTemp[0] = tg + (tg/4); \
}

enum TCS_Modes {
      TCS_AUTO         =  0 // авторежим: коррекция + усреднение + люксометр + вн.баланс + цв. контраст + автоподстройка скорости
    , TCS_NOT_CORRECT  =  1 // режим без коррекции данных
    , TCS_NOT_AVERAGE  =  2 // режим без усреднения результатов
    , TCS_NOT_LUKSES   =  4 // режим без перевода яркости в люксы
    , TCS_NOT_WHITE    =  8 // режим без коррекции баланса от вн. светодиодов
    , TCS_NOT_CONTRAST = 16 // режим без повышения цветового контраста
    , TCS_SPEED_CONST  = 32 // режим без адаптации скорости работы (всегда фикс.)
    , TCS_WB_ONLY      = 64 // режим замера только яркостного канала! -коррекция -усреднение -люксометр -вн.баланс -цв. контраст
};

extern volatile int      tcsColors[TCS_MAX_SENSORS][4]; // итоговые и усредненные данные замера
extern volatile int      tcsTemp[4];                    // внутреннее хранение данных при усреднениях и замерах
extern volatile int      minVal,maxVal;                 // мин и макс. уровни каналов цвета
extern volatile uint16_t tcsCount;                      // номер завершенного замера
extern volatile uint8_t  tcsColor;                      // текущий измеряемый цвет в попытке или состояние КА
extern volatile uint8_t  tcsMeasure;                    // номер текущей усредняемой попытки замера
extern volatile uint8_t  tcsWait;                       // текущая длительность одного замера частоты от датчика
extern volatile uint8_t  tcsIsBright;                   // Пересвет? (и прочие возвращаемые статусы на будущее)
extern volatile uint8_t  tcsModes;                      // Биты определяют режимы работы драйвера
extern volatile uint8_t  tcsCurrent;                    // Текущий датчик в режиме 2-х и более датчиков
extern volatile uint8_t  tcsCurWait;                    // пропуск заданного числа прерываний таймера @see tcsRun()

TimerHookProc tcsSetup();               // Настройка таймера и выводов на работу с датчиком @global tcsWait
void          tcsPrepare(uint8_t num);  // настройка заданного датчика на чтение данных о цвете @global tcsColor,tcsCurrent
void          tcsNextColor();           // переключение датчика на текущий цвет и сброс таймера для нового замера @global tcsColor
void          tcsCorrect();             // Коррекция показаний по усредненной обратной матрице "среднее" из даташит: @global tcsTemp[]
void          tcsMinMax();              // Подсчет мин/мах уровней каналов цвета @global int minVal,maxVal,tcsTemp[]
void          tcsWhiteLocal();          // коррекция баланса встроенных светодиодов @global tcsTemp[]
void          tcsContrast();            // повышение контраста @global minVal,maxVal,tcsTemp[]
void          tcsRun();                 // КА датчика: замеры цвета, пересчеты, коррекции и сохранение результата

#ifdef __cplusplus
  } // extern "C"
#endif

#endif // _TCS3200_
