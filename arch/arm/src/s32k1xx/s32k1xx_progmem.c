/******************************************************************************
 * arch/arm/src/s32k1xx/s32k1xx_progmem.c
 *
 *   Copyright (C) 2019 Gregory Nutt. All rights reserved.
 *   Author:  Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/******************************************************************************
 * Included Files
 ******************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "up_arch.h"

#include "hardware/s32k1xx_ftfc.h"

#include "s32k1xx_config.h"
#include "s32k1xx_progmem.h"

#include "up_internal.h"

#include <arch/board/board.h> /* Include last:  has dependencies */

/******************************************************************************
 * Pre-processor Definitions
 ******************************************************************************/
#ifdef CONFIG_MTD_SMART
# ifndef CONFIG_MTD_SMART_ENABLE_CRC
#  error SmartFS CRC has to be enbabled with this driver
# endif
#endif

/******************************************************************************
 * Private Data
 ******************************************************************************/

union fccob_flash_addr
{
    uint32_t addr;
    struct
    {
        uint8_t fccob3;
        uint8_t fccob2;
        uint8_t fccob1;
        uint8_t pad;
    } fccobs;
};

/******************************************************************************
 * Private Functions
 ******************************************************************************/

static inline void wait_ftfc_ready()
{
  while ((getreg8(S32K1XX_FTFC_FSTAT) & FTTC_FSTAT_CCIF) == 0)
    {
      /* Busy */
    }
}

static uint32_t execute_ftfc_command()
{
  uint8_t regval;
  uint32_t retval = 0;

  /* Clear CCIF to launch command */

  regval = getreg8(S32K1XX_FTFC_FSTAT);
  regval |= FTTC_FSTAT_CCIF;
  putreg8(regval, S32K1XX_FTFC_FSTAT);

  wait_ftfc_ready();

  retval = getreg8(S32K1XX_FTFC_FSTAT);

  if (retval & (FTTC_FSTAT_MGSTAT0 | FTTC_FSTAT_FPVIOL |
                FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR))
    {
      return retval; /* Error has occured */
    }

  return retval;
}

/******************************************************************************
 * Public Functions
 ******************************************************************************/

/******************************************************************************
 * Name: up_progmem_neraseblocks
 *
 * Description:
 *   Return number of erase blocks
 *
 ******************************************************************************/

size_t up_progmem_neraseblocks(void)
{
  return S32K1XX_PROGMEM_SECTOR_COUNT;
}

/******************************************************************************
 * Name: up_progmem_isuniform
 *
 * Description:
 *   Is program memory uniform or page size differs?
 *
 ******************************************************************************/

bool up_progmem_isuniform(void)
{
  return true;
}

/******************************************************************************
 * Name: up_progmem_pagesize
 *
 * Description:
 *   Return read/write page size
 *
 ******************************************************************************/

size_t up_progmem_pagesize(size_t page)
{
  return (size_t)S32K1XX_PROGMEM_PAGE_SIZE;
}

/******************************************************************************
 * Name: up_progmem_erasesize
 *
 * Description:
 *   Return erase block size
 *
 ******************************************************************************/

size_t up_progmem_erasesize(size_t block)
{
  return (size_t)S32K1XX_PROGMEM_BLOCK_SECTOR_SIZE;
}

/******************************************************************************
 * Name: up_progmem_getpage
 *
 * Description:
 *   Address to read/write page conversion
 *
 * Input Parameters:
 *   addr - Address with or without flash offset (absolute or aligned to page0)
 *
 * Returned Value:
 *   Page or negative value on error.  The following errors are reported
 *   (errno is not set!):
 *
 *     -EFAULT: On invalid address
 *
 ******************************************************************************/

ssize_t up_progmem_getpage(size_t addr)
{
  if (addr >= S32K1XX_PROGMEM_START_ADDR)
    {
      addr -= S32K1XX_PROGMEM_START_ADDR;
    }

  return (size_t)(addr / S32K1XX_PROGMEM_PAGE_SIZE);
}

/******************************************************************************
 * Name: up_progmem_getaddress
 *
 * Description:
 *   Read/write page to address conversion
 *
 * Input Parameters:
 *   page - page index
 *
 * Returned Value:
 *   Base address of given page, SIZE_MAX if page index is not valid.
 *
 ******************************************************************************/

size_t up_progmem_getaddress(size_t page)
{
  return (size_t)(S32K1XX_PROGMEM_START_ADDR
           + (page * S32K1XX_PROGMEM_PAGE_SIZE));
}

/******************************************************************************
 * Name: up_progmem_eraseblock
 *
 * Description:
 *   Erase selected block.
 *
 * Input Parameters:
 *   block - The erase block index to be erased.
 *
 * Returned Value:
 *   block size or negative value on error.  The following errors are reported
 *   (errno is not set!):
 *
 *     -EFAULT: On invalid page
 *     -EIO:    On unsuccessful erase
 *     -EROFS:  On access to write protected area
 *     -EACCES: Insufficient permissions (read/write protected)
 *     -EPERM:  If operation is not permitted due to some other constraints
 *              (i.e. some internal block is not running etc.)
 *
 ******************************************************************************/

ssize_t up_progmem_eraseblock(size_t block)
{
  union fccob_flash_addr dest;

  dest.addr = (block * S32K1XX_PROGMEM_BLOCK_SECTOR_SIZE) + 0x800000
      - S32K1XX_PROGMEM_START_ADDR;

  wait_ftfc_ready();

  /* Clear FSTAT error bits */

  putreg8(FTTC_FSTAT_FPVIOL | FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR,
           S32K1XX_FTFC_FSTAT);

  putreg8(S32K1XX_FTFC_ERASE_SECTOR, S32K1XX_FTFC_FCCOB0);

  putreg8(dest.fccobs.fccob1, S32K1XX_FTFC_FCCOB1);
  putreg8(dest.fccobs.fccob2, S32K1XX_FTFC_FCCOB2);
  putreg8(dest.fccobs.fccob3, S32K1XX_FTFC_FCCOB3);

  if (execute_ftfc_command() & (FTTC_FSTAT_MGSTAT0 | FTTC_FSTAT_FPVIOL |
      FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR))
    {
      return -EIO; /* Error has occured */
    }

#ifdef FTFC_VERIFY_CHECK
  wait_ftfc_ready();

  putreg8(FTTC_FSTAT_FPVIOL | FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR,
           S32K1XX_FTFC_FSTAT);

  putreg8(S32K1XX_FTFC_VERIFY_SECTION, S32K1XX_FTFC_FCCOB0);

  putreg8(dest.fccobs.fccob1, S32K1XX_FTFC_FCCOB1);
  putreg8(dest.fccobs.fccob2, S32K1XX_FTFC_FCCOB2);
  putreg8(dest.fccobs.fccob3, S32K1XX_FTFC_FCCOB3);
  putreg8(1, S32K1XX_FTFC_FCCOB4); /* 2048 / 8 = 256 */
  putreg8(0, S32K1XX_FTFC_FCCOB5);
  putreg8(1, S32K1XX_FTFC_FCCOB6);

  if (execute_ftfc_command() & (FTTC_FSTAT_MGSTAT0 | FTTC_FSTAT_FPVIOL |
      FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR))
    {
      return -EIO; /* Error has occured */
    }
#endif

  return (ssize_t)S32K1XX_PROGMEM_BLOCK_SECTOR_SIZE;
}

/******************************************************************************
 * Name: up_progmem_ispageerased
 *
 * Description:
 *   Checks whether page is erased
 *
 * Input Parameters:
 *   page - The erase page index to be checked.
 *
 * Returned Value:
 *   Returns number of bytes NOT erased or negative value on error. If it
 *   returns zero then complete page is erased.
 *
 *   The following errors are reported:
 *     -EFAULT: On invalid page
 *
 ******************************************************************************/

ssize_t up_progmem_ispageerased(size_t page)
{
  const uint8_t *p;
  int i;

  if (page >= S32K1XX_PROGMEM_PAGE_COUNT)
    {
      return -EFAULT;
    }

  p = (const uint8_t *)up_progmem_getaddress(page);

  for (i = 0; i < S32K1XX_PROGMEM_PAGE_SIZE; i++)
    {
      if (p[i] != 0xff)
        {
          break;
        }
    }

  return (ssize_t)(S32K1XX_PROGMEM_PAGE_SIZE - i);
}

/******************************************************************************
 * Name: up_progmem_write
 *
 * Description:
 *   Program data at given address
 *
 *   Note: this function is not limited to single page and nor it requires
 *   the address be aligned inside the page boundaries.
 *
 * Input Parameters:
 *   addr  - Address with or without flash offset
 *   buf   - Pointer to buffer
 *   count - Number of bytes to write
 *
 * Returned Value:
 *   Bytes written or negative value on error.  The following errors are
 *   reported (errno is not set!)
 *
 *     EINVAL: If count is not aligned with the flash boundaries (i.e.
 *             some MCU's require per half-word or even word access)
 *     EFAULT: On invalid address
 *     EIO:    On unsuccessful write
 *     EROFS:  On access to write protected area
 *     EACCES: Insufficient permissions (read/write protected)
 *     EPERM:  If operation is not permitted due to some other constraints
 *             (i.e. some internal block is not running etc.)
 *
 ******************************************************************************/

ssize_t up_progmem_write(size_t addr, FAR const void *buf, size_t count)
{
  union fccob_flash_addr dest;
  uint32_t temp;
  uint32_t i;
  uint32_t j;
  uint8_t *src;

  if (addr >= S32K1XX_PROGMEM_START_ADDR)
    {
      addr -= S32K1XX_PROGMEM_START_ADDR;
    }

  if (count % S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE != 0)
    {
      return -EINVAL;
    }

#ifdef SMART_FS_DOUBLE_WRITE_WORKAROUND
  if (*(uint32_t *)addr == *(uint32_t *)buf
      && *(uint32_t *)addr + 4 == *(uint32_t *)buf + 4)
    {
      return count;
    }
#endif

  src = (uint8_t *)buf;
  dest.addr = addr + 0x800000;

  for (i = 0; i < count / S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE ; i++)
    {
      wait_ftfc_ready();

      /* Clear FSTAT error bits */

      putreg8(FTTC_FSTAT_FPVIOL | FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR,
               S32K1XX_FTFC_FSTAT);

      putreg8(S32K1XX_FTFC_PROGRAM_PHRASE, S32K1XX_FTFC_FCCOB0);

      putreg8(dest.fccobs.fccob1, S32K1XX_FTFC_FCCOB1);
      putreg8(dest.fccobs.fccob2, S32K1XX_FTFC_FCCOB2);
      putreg8(dest.fccobs.fccob3, S32K1XX_FTFC_FCCOB3);

      for (j = 0; j < S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE; j++)
        {
          temp = S32K1XX_FTFC_BASE + j + 0x8;
          *(volatile uint8_t *)(temp) = src[j];
        }

      if (execute_ftfc_command() & (FTTC_FSTAT_MGSTAT0 | FTTC_FSTAT_FPVIOL |
          FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR))
        {
          return -EIO; /* Error has occured */
        }

      dest.addr = dest.addr + S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE;
      src = src + S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE;
    }

#ifdef FTFC_VERIFY_CHECK
  src = (uint8_t *)buf;
  dest.addr = addr + 0x800000;

  for (i = 0; i < count / S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE ; i++)
    {
      wait_ftfc_ready();

      /* Clear FSTAT error bits */

      putreg8(FTTC_FSTAT_FPVIOL | FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR,
               S32K1XX_FTFC_FSTAT);

      putreg8(S32K1XX_FTFC_PROGRAM_CHECK, S32K1XX_FTFC_FCCOB0);

      putreg8(dest.fccobs.fccob1, S32K1XX_FTFC_FCCOB1);
      putreg8(dest.fccobs.fccob2, S32K1XX_FTFC_FCCOB2);
      putreg8(dest.fccobs.fccob3, S32K1XX_FTFC_FCCOB3);
      putreg8(1, S32K1XX_FTFC_FCCOB4); /* Margin level 1 */

      for (j = 0; j < S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE; j++)
        {
          temp = S32K1XX_FTFC_BASE + j + 0xc;
          *(uint8_t *)(temp) = src[j];
        }

      if (execute_ftfc_command() & (FTTC_FSTAT_MGSTAT0 | FTTC_FSTAT_FPVIOL |
          FTTC_FSTAT_ACCERR | FTTC_FSTAT_RDCOLERR))
        {
          return count; /* Error has occured */
        }

       dest.addr = dest.addr + S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE;
       src = src + S32K1XX_PROGMEM_DFLASH_WRITE_UNIT_SIZE;
    }
#endif

  return count;
}

void s32k1xx_progmem_init()
{
  /* Disable D-Flash Cache */

  putreg32(0xc706b030, S32K1XX_MSCM_BASE + 0x404);

  /* Setup D-flash partitioning */

  putreg8(S32K1XX_FTFC_PROGRAM_PARTITION, S32K1XX_FTFC_FCCOB0); /* Command */

  putreg8(0x0, S32K1XX_FTFC_FCCOB1); /* CSEc key size */
  putreg8(0x0, S32K1XX_FTFC_FCCOB2); /* uSFE */
  putreg8(0x0, S32K1XX_FTFC_FCCOB3); /* Disable FlexRAM EEE */
  putreg8(0xf, S32K1XX_FTFC_FCCOB4); /* EEE Partition code */
  putreg8(0x0, S32K1XX_FTFC_FCCOB5); /* DE  Partition code */

  execute_ftfc_command();
}
