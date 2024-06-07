#ifndef BURST_H
#define BURST_H

#define ARRAY_SIZE 16

struct BurstItem
{
    uint16_t dec;
    uint16_t count;
};

struct BurstArray
{
    struct BurstItem array[ARRAY_SIZE];
    size_t size;
};

struct BurstArray lineBurst[2];

// struct BurstArray data = {.size = 0};

void addOrUpdate(struct BurstArray *data, uint16_t dec)
{
    size_t i;
    // Szukanie elementu z podaną wartością dec
    for (i = 0; i < data->size; i++)
    {
        if (data->array[i].dec == dec)
        {
            data->array[i].count++;
            break;
        }
    }

    // Jeśli element nie istnieje, dodajemy nową strukturę
    if (i == data->size)
    {
        if (data->size < ARRAY_SIZE)
        {
            data->array[data->size].dec = dec;
            data->array[data->size].count = 1;
            data->size++;
        }
        else
        {
            return;
        }
    }

    // Wstawianie elementu w odpowiednie miejsce, aby tablica była posortowana
    struct BurstItem temp = data->array[i];
    size_t j = i;
    while (j > 0 && data->array[j - 1].count < temp.count)
    {
        data->array[j] = data->array[j - 1];
        j--;
    }
    data->array[j] = temp;
}

static inline void burst_clear()
{
    lineBurst[0].size = 0;
    lineBurst[1].size = 0;
}

#define VAL_F(x) (x & 0x1)
#define VAL_A(x) ((x >> 1) & 0x1f)
#define VAL_B(x) ((x >> 6) & 0x1f)

static inline void burst_analyze(int row)
{

    if (frame_count % 500 == 0)
    {
        // output data
        uint16_t dec0 = lineBurst[row % 2].array[0].dec;
        uint16_t dec1 = lineBurst[row % 2].array[1].dec;

        sprintf(buf, "burst %d %d.%d.%d->%d, %d.%d.%d->%d", row % 2, VAL_F(dec0), VAL_A(dec0), VAL_B(dec0), lineBurst[row % 2].array[0].count, VAL_F(dec1), VAL_A(dec1), VAL_B(dec1), lineBurst[row % 2].array[1].count);
        uart_log_putln(buf);

        burst_clear();
    }
    for (int i = 0; i < 3; i++)
        addOrUpdate(&lineBurst[row % 2], decode_intr(chroma_buf[buf_seq][2 + i * 4]));
}

#endif