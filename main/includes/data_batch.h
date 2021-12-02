#define BUFFER_RECORDS 50*4*2 //50 Hz, 4 seconds ~ 5 KB. Each message is 24 bytes 

void data_batch_task(void *args);