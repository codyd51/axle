interpret all the grub fields and do something with the data

Bugs
---------
GRUB RAM section map + PMM system RAM maps don't look right when using 4GB RAM. 
Bug may exist for RAM values < 4GB too.
128mb ram exhibits correct behavior

Code Changes
---------
uroboro
    `if (system_frames_entry & (1 << j) != RESERVED) {`
    `if (pmm_frames_entry & (1 << j) != ALLOCATED) {`
codyd51 
    or IS_BIT_ALLOCATED(entry, bit)
uroboro
    whatever floats your boat, as long as you don't have to assume 0 being not used or not allocated or whatnot
codyd51 
    ok ill add some definition