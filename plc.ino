#include <Adafruit_MCP23X17.h>
#include <LiquidCrystal.h>

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 8, en = 9;
LiquidCrystal lcd( rs, en, 4, 5, 6, 7 );
Adafruit_MCP23X17 mcp;

// clang-format off
byte xioChar[8] = {
  0b11011,
  0b01010,
  0b01010,
  0b01010,
  0b01010,
  0b01010,
  0b11011,
  0b00000
};
byte xicChar[8] = {
  0b11011,
  0b01010,
  0b01011,
  0b01110,
  0b11010,
  0b01010,
  0b11011,
  0b00000
};

byte oteChar[8] = {
  0b01010,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b10001,
  0b01010,
  0b00000
};

byte brChar[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00100
};

byte brOpenChar[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b00111,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

byte brCloseChar[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b11100,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

// clang-format on

static const int BUTTON_VALUE_LIST[] = {
    0,
    204,
    405,
    620,
    820,
    1023,
};

enum {
    BUTTON_RIGHT = 0,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_SELECT,
    BUTTON_NONE,
};

static enum {
    LOOP_LADDER,
    LOOP_IO,
    LOOP_RUN,
} loop_id;

static uint8_t lastIoA = 0;
static uint8_t lastIoB = 0;

static void render_io()
{
    lcd.setCursor( 0, 0 );
    lcd.print( "0123456789abcdef" );
    lcd.setCursor( 0, 1 );
    for ( int i = 0; i < 8; i++ )
        lcd.print( ( lastIoA >> i & 1 ) ? '1' : '.' );
    for ( int i = 0; i < 8; i++ )
        lcd.print( ( lastIoB >> i & 1 ) ? '1' : '.' );
}

static void loop_io()
{
    uint8_t ioA = ~mcp.readGPIO( 0 );
    uint8_t ioB = ~mcp.readGPIO( 1 );

    if ( ioA != lastIoA || ioB != lastIoB ) {
        lastIoA = ioA;
        lastIoB = ioB;
        render_io();
    }

    delay( 50 );
}

static void loop_ladder()
{
    // if (loop_id == LOOP_RUN) loop_io();

    // read the potentiometer on A0:
    static int last = 0;
    int sensorReading = analogRead( A0 );
    // map the result to 200 - 1000:
    int delayTime = 50;

    //  if (sensorReading != last) {
    //    lcd.setCursor(10, 1);
    //    lcd.print("      ");
    //    lcd.setCursor(10, 1);
    //    lcd.print(sensorReading);
    //
    //    last = sensorReading;
    //  }

    static int cx = 0;
    static int cy = 0;

    static int last_button = BUTTON_NONE;
    int button = read_button();

    if ( last_button == BUTTON_NONE ) {
        if ( button == BUTTON_LEFT )
            cx--;
        if ( button == BUTTON_RIGHT )
            cx++;
        if ( button == BUTTON_UP )
            cy--;
        if ( button == BUTTON_DOWN )
            cy++;
        cx = constrain( cx, 0, 15 );
        cy = constrain( cy, 0, 1 );
        if ( button == BUTTON_SELECT ) {
            mcp.digitalWrite( 1, LOW );
            static int toggle = 0;
            if ( toggle )
                lcd.noBlink();
            if ( !toggle )
                lcd.blink();
            if ( toggle )
                mcp.digitalWrite( 0, HIGH );
            if ( !toggle )
                mcp.digitalWrite( 0, LOW );
            toggle = ( toggle + 1 ) % 2;
        } else {
            mcp.digitalWrite( 1, HIGH );
        }
        if ( button != BUTTON_NONE )
            lcd.setCursor( cx, cy );
    }

    last_button = button;

    delay( delayTime );
}

void setup()
{
    // initialize LCD and set up the number of columns and rows:
    lcd.begin( 16, 2 );

    if ( !mcp.begin_I2C( 0x27 ) ) {
        lcd.print( "i2c error" );
        while ( 1 )
            ;
    }

    //  mcp.pinMode(0, OUTPUT);
    //  mcp.pinMode(1, OUTPUT);

    for ( int i = 0; i < 16; i++ ) {
        mcp.pinMode( i, INPUT_PULLUP );
    }

    // mcp.digitalWrite(0, HIGH);
    // mcp.digitalWrite(1, HIGH);

    lcd.createChar( 0, xioChar );
    lcd.createChar( 1, xicChar );
    lcd.createChar( 2, oteChar );
    lcd.createChar( 3, brChar );
    lcd.createChar( 4, brOpenChar );
    lcd.createChar( 5, brCloseChar );
    // lcd.blink();
    lcd.cursor();

    lcd.setCursor( 0, 0 );
    lcd.write( byte( 0 ) );
    lcd.write( '0' );
    lcd.write( byte( 3 ) );
    // lcd.write('-');
    lcd.write( byte( 1 ) );
    lcd.write( '1' );
    lcd.write( byte( 3 ) );
    lcd.print( "--------" );
    lcd.write( 'a' );
    lcd.write( byte( 2 ) );
    lcd.setCursor( 2, 1 );
    lcd.write( byte( 4 ) );
    // lcd.write('-');
    lcd.write( byte( 0 ) );
    lcd.write( 'a' );
    lcd.write( byte( 5 ) );
    // lcd.setCursor(14, 2);
    // lcd.print("R1");
    lcd.setCursor( 0, 0 );

    render_io();
}

static int read_button()
{
    int value = analogRead( A0 );
    int best = 0;

    for ( int i = 0; i < 6; i++ ) {
        if ( abs( value - BUTTON_VALUE_LIST[ i ] ) <
             abs( value - BUTTON_VALUE_LIST[ best ] ) ) {
            best = i;
        }
    }
    return best;
}

void loop()
{
    if ( loop_id == LOOP_LADDER )
        loop_ladder();
    if ( loop_id == LOOP_IO )
        loop_io();
}
