#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"

static int is_active = 0;

int mdadm_mount(void) {
  //check multiple mounts
  if (is_active == 1){
    return -1;
  }
  //create mount command
  uint32_t command = (JBOD_MOUNT << 12);
  int status = jbod_operation(command, NULL);
  if (status == 0){
    is_active = 1;
    return 1; // successful mount
  }
  return -1; // unsuccessful mount
}

int mdadm_unmount(void) {
  // Check if system mounted before unmounting
  if (is_active == 0){
    return -1;
  }
  // create unmount command and execute
  uint32_t command = (JBOD_UNMOUNT << 12);
  int status = jbod_operation(command, NULL);
  if (status == 0){
    is_active = 0;
    return 1; //successful unmount
  }
  return -1; //Unsuccessful unmount
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
  if (is_active == 0) return -3;
  if (start_addr + read_len > JBOD_DISK_SIZE * JBOD_NUM_DISKS) return -1;
  if (read_len > 1024) return -2;
  if (read_len > 0 && read_buf == NULL) return -4;

  uint32_t total_read = 0;
  uint32_t current_pos = start_addr;

  //
  while (total_read < read_len){
    //calculate disk block and offset for current position
    uint32_t disk_num = current_pos / JBOD_DISK_SIZE;
    uint32_t block_num = (current_pos % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    uint32_t block_offset = (current_pos % JBOD_BLOCK_SIZE);

    //seek to according disk
    uint32_t command = (JBOD_SEEK_TO_DISK << 12) | disk_num;
    if (jbod_operation(command,NULL)==-1){
      return -1;
  }
    //seek to correct block on disk
    command = (JBOD_SEEK_TO_BLOCK << 12) | (block_num & 0xFF) << 4;
    if (jbod_operation(command, NULL) == -1){
      return -1;
    }

    //read entire block
    uint8_t block_data[JBOD_BLOCK_SIZE];
    command = (JBOD_READ_BLOCK << 12);
    if (jbod_operation(command, block_data) == -1){
      return -1;
    }

    //Determine data size to copy from current block
    uint32_t bytes_to_copy = JBOD_BLOCK_SIZE - block_offset;
    if (bytes_to_copy > read_len - total_read) {
      bytes_to_copy = read_len - total_read;
    }

    //copy according data from block to output buffer
    memcpy(read_buf + total_read, block_data + block_offset, bytes_to_copy);
    //update progress trackers
    total_read += bytes_to_copy;
    current_pos += bytes_to_copy;
  }
  return total_read;
}

