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

byte doubleBrChar[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b01010,
  0b01010,
  0b01010,
  0b01010
};

byte brSplitChar[8] = {
  0b01010,
  0b01010,
  0b01010,
  0b11011,
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
    CHAR_XIO = 0,
    CHAR_XIC,
    CHAR_OTE,
    CHAR_BR,
    CHAR_BR_OPEN,
    CHAR_BR_CLOSE,
    CHAR_BR_SPLIT,
    CHAR_DOUBLE_BR,
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
    LOOP_LADDER = 0,
    LOOP_IO,
    LOOP_RUN,
} loop_id;

static uint8_t lastIoA = 0;
static uint8_t lastIoB = 0;

static const char * IO_NAME_LIST = "abcdefghABCDEFGH";

enum {
  CMD_NULL = 0,
  CMD_BR,
  CMD_XIO,
  CMD_XIC,
  CMD_OTE,
};

static int * cmd_grid;
static int * data_grid;

static int program_width = 16;
static int program_height = 8;

static int * rung_cmd_list( int rung )
{
    return cmd_grid + rung * program_width;
}

static int * rung_data_list( int rung )
{
    return data_grid + rung * program_width;
}

static void render_io()
{
    lcd.setCursor( 0, 0 );
    lcd.print( IO_NAME_LIST );
    lcd.setCursor( 0, 1 );
    for ( int i = 0; i < 8; i++ )
        lcd.print( ( lastIoA >> i & 1 ) ? '1' : '.' );
    for ( int i = 0; i < 8; i++ )
        lcd.print( ( lastIoB >> i & 1 ) ? '1' : '.' );
}

// ..-..-..-..-..-0
// ................

enum { 
    SEG_NO_BR = 0,
    SEG_OPEN_BR,
    SEG_CLOSE_BR,
    SEG_OPENCLOSE_BR,
    SEG_BR,
};

static struct {
    int cmd;
    int data;

    int branch;

    int branch_cmd;
    int branch_data;

} segment_list[5];

static void setup_ladder( int rung ) 
{
    int * cmd_list = rung_cmd_list( rung );
    int * data_list = rung_data_list( rung );

    for ( int i = 0; i < 5; i++ ) {
        segment_list[ i ].cmd = CMD_NULL;
        segment_list[ i ].branch_cmd = CMD_NULL;
        segment_list[ i ].branch = SEG_NO_BR;
    }


    int x = 0;
    int i = 1;

    int br_state = 0;
    int br_x = 0;

    while ( cmd_list[ i ] != CMD_NULL ) {
        int cmd = cmd_list[ i ];

        if ( br_state == 0 && cmd == CMD_BR ) {
            br_state = 1;
            br_x = x;
            segment_list[ x ].branch = SEG_OPEN_BR;
            i++;
            continue;
        }

        if ( br_state == 1 && cmd == CMD_BR ) {
            br_state = 2;
            i++;
            continue;
        }

        if ( br_state == 2 && cmd == CMD_BR ) {
            br_state = 0;

            int last = max( x, br_x ) - 1;
            if ( segment_list[ last ].branch == SEG_OPEN_BR ) {
                segment_list[ last ].branch = SEG_OPENCLOSE_BR;
            } else {
                segment_list[ last ].branch = SEG_CLOSE_BR;
            }
            x = last + 1;
            i++;
            continue;
        }

        if ( br_state != 2 ) {
            segment_list[ x ].cmd = cmd;
            segment_list[ x ].data = data_list[ i ];
            if ( br_state == 1 && segment_list[ x ].branch == SEG_NO_BR ) {
                segment_list[ x ].branch = SEG_BR;
            }
            x++;
        } else {
            segment_list[ br_x ].branch_cmd = cmd;
            segment_list[ br_x ].branch_data = data_list[ i ];
            if ( segment_list[ br_x ].branch == SEG_NO_BR ) {
                segment_list[ br_x ].branch = SEG_BR;
            }
            br_x++;
        }

        i++;
    }
}

static void render_ladder( int rung )
{
    setup_ladder( rung );

    lcd.setCursor( 0, 0 );

    for ( int i = 0; i < 5; i++ ) {
        lcd.setCursor( i * 3, 0 );
        lcd.print( "---" );
        lcd.setCursor( i * 3, 0 );

        //lcd.print( segment_list[i].cmd );
        if ( segment_list[ i ].cmd == CMD_XIO ) {
            lcd.write( CHAR_XIO );
        }
        if ( segment_list[ i ].cmd == CMD_XIC ) {
            lcd.write( CHAR_XIC );
        }

        if ( segment_list[ i ].branch != SEG_NO_BR ) {
            lcd.setCursor( i * 3, 1 );
            lcd.print( "---" );
            lcd.setCursor( i * 3, 1 );

            if ( segment_list[ i ].branch_cmd == CMD_XIO ) {
                lcd.write( CHAR_XIO );
            }
            if ( segment_list[ i ].branch_cmd == CMD_XIC ) {
                lcd.write( CHAR_XIC );
            }
        }

        int branch = segment_list[ i ].branch;

        if ( i > 0 && ( branch == SEG_OPEN_BR || branch == SEG_OPENCLOSE_BR ) ) {
            if ( segment_list[ i - 1 ].branch == SEG_NO_BR ) {
                lcd.setCursor( i * 3 - 1, 0 );
                lcd.write( CHAR_BR );
                lcd.setCursor( i * 3 - 1, 1 );
                lcd.write( CHAR_BR_OPEN );
            } else {
                lcd.setCursor( i * 3 - 1, 0 );
                lcd.write( CHAR_DOUBLE_BR );
                lcd.setCursor( i * 3 - 1, 1 );
                lcd.write( CHAR_BR_SPLIT );
            }
        }

        if ( branch == SEG_CLOSE_BR || branch == SEG_OPENCLOSE_BR ) {
            lcd.setCursor( i * 3 + 2, 0 );
            lcd.write( CHAR_BR );
            lcd.setCursor( i * 3 + 2, 1 );
            lcd.write( CHAR_BR_CLOSE );
        }

    }
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
    cmd_grid = new int[ program_width * program_height ];
    data_grid = new int[ program_width * program_height ];
    
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

    lcd.createChar( CHAR_XIO, xioChar );
    lcd.createChar( CHAR_XIC, xicChar );
    lcd.createChar( CHAR_OTE, oteChar );
    lcd.createChar( CHAR_BR, brChar );
    lcd.createChar( CHAR_BR_OPEN, brOpenChar );
    lcd.createChar( CHAR_BR_CLOSE, brCloseChar );
    lcd.createChar( CHAR_BR_SPLIT, brSplitChar );
    lcd.createChar( CHAR_DOUBLE_BR, doubleBrChar );

    // lcd.blink();
    lcd.cursor();
    lcd.setCursor( 0, 0 );

    int * cmd_list = rung_cmd_list( 0 );
    cmd_list[ 0 ] = CMD_OTE;
    cmd_list[ 1 ] = CMD_XIO;
    cmd_list[ 2 ] = CMD_BR;
    cmd_list[ 3 ] = CMD_XIO;
    cmd_list[ 4 ] = CMD_XIO;
    cmd_list[ 5 ] = CMD_XIO;
    cmd_list[ 6 ] = CMD_BR;
    cmd_list[ 7 ] = CMD_XIO;
    cmd_list[ 8 ] = CMD_BR;
    cmd_list[ 9 ] = CMD_NULL;

    render_ladder( 0 );
    //lcd.write( CHAR_XIC );


}



void loop()
{
    if ( loop_id == LOOP_LADDER )
        loop_ladder();
    if ( loop_id == LOOP_IO )
        loop_io();
}
