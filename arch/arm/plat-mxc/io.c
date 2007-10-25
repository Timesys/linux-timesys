/*
 * MX27/MX31 custom ioremap implementation.
 *
 *  Copyright 2007 Sony Corporation.
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/hardware.h>
#include <asm/io.h>

void __iomem *__mxc_ioremap(unsigned long cookie, size_t size,
	unsigned int mtype)
{
	unsigned long addr;
	void __iomem *retval;

	addr = IO_ADDRESS(cookie);

#ifdef DEBUG
	__print_symbol("%s ", __builtin_return_address(0));
	printk("called %s(): ", __FUNCTION__);
#endif

	if (addr != 0xdeadbeef) {
		retval = (void __iomem *)addr;
#ifdef DEBUG
		printk("0x%08lx -> 0x%p\n", cookie, retval);
#endif
	} else {
		retval = __arm_ioremap(cookie, size, mtype);
#ifdef DEBUG
		printk("Not using static map: 0x%08lx -> 0x%p\n", cookie, retval);
#endif
	}

	return retval;
}
EXPORT_SYMBOL(__mxc_ioremap);

void __mxc_iounmap(void __iomem *addr)
{
	if (IS_STATIC_MAPPED((unsigned long)addr))
		return;
	__iounmap(addr);
}
EXPORT_SYMBOL(__mxc_iounmap);



