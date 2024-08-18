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
    for (int j = 0; j < 2; j++)
    {
        for (int i = 0; i < lineBurst[j].size; i++)
        {
            lineBurst[j].array[i].dec = 0;
            lineBurst[j].array[i].count = 0;
        }
        lineBurst[j].size = 0;
    }
}

#define VAL_F(x) (x & 0x1)
#define VAL_A(x) ((x >> 1) & 0x1f)
#define VAL_B(x) ((x >> 6) & 0x1f)

struct _Pair
{
    uint value;
    uint weight;
};

float weighted_average(struct _Pair *array)
{
    int sum_weights = 0;
    int sum_values = 0;

    // Oblicz sumę ważoną i sumę wartości
    for (int i = 0; i < 3; i++)
    {
        sum_weights += array[i].value * array[i].weight;
        sum_values += array[i].weight;
        array[i].weight = 0;
        array[i].value = 0;
    }

    // Jeżeli suma wartości jest większa od zera, oblicz średnią ważoną
    if (sum_values != 0)
    {
        return (float)sum_weights / sum_values;
    }
    else
    {
        return 0.0; // Unikamy dzielenia przez zero
    }
}

static inline void burst_analyze(int row)
{
    struct _Pair tmp_burst[3];

    if (row != 310)
        return;

    if (frame_count % 500 == 0)
    {
        // output data
        uint16_t dec0 = lineBurst[row % 2].array[0].dec;
        uint16_t dec1 = lineBurst[row % 2].array[1].dec;
        uint16_t dec2 = lineBurst[row % 2].array[2].dec;

    //    uart_log_putlnf("burst %d %d.%d.%d->%d, %d.%d.%d->%d, %d.%d.%d->%d", row % 2, 
        // VAL_F(dec0), VAL_A(dec0), VAL_B(dec0), lineBurst[row % 2].array[0].count, 
        // VAL_F(dec1), VAL_A(dec1), VAL_B(dec1), lineBurst[row % 2].array[1].count,
        // VAL_F(dec2), VAL_A(dec2), VAL_B(dec2), lineBurst[row % 2].array[2].count);



        tmp_burst[0].value = VAL_A(dec0);
        tmp_burst[0].weight = lineBurst[row % 2].array[0].count;
        tmp_burst[1].value = VAL_A(dec1);
        tmp_burst[1].weight = lineBurst[row % 2].array[1].count;
        tmp_burst[2].value = VAL_A(dec2);
        tmp_burst[2].weight = lineBurst[row % 2].array[2].count;

        float bf_a = weighted_average(tmp_burst);

        tmp_burst[0].value = VAL_B(dec0);
        tmp_burst[0].weight = lineBurst[row % 2].array[0].count;
        tmp_burst[1].value = VAL_B(dec1);
        tmp_burst[1].weight = lineBurst[row % 2].array[1].count;
        tmp_burst[2].value = VAL_B(dec2);
        tmp_burst[2].weight = lineBurst[row % 2].array[2].count;

        float bf_b = weighted_average(tmp_burst);

        uart_log_putlnf("burst: %d %02.02f %02.02f", VAL_F(dec0), bf_a, bf_b);

        
        burst_clear();
    }
    for (int i = 0; i < 3; i++)
        addOrUpdate(&lineBurst[row % 2], decode_intr(chroma_buf[buf_seq][0 + i * 4]));
}

#endif