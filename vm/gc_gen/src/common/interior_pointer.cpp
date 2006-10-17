
#include "interior_pointer.h"
void gc_add_root_set_entry(Managed_Object_Handle *ref, Boolean is_pinned);

typedef struct slot_offset_entry_struct{
	void** slot;
	unsigned int offset;
} slot_offset_entry;

static std::vector<slot_offset_entry> interior_pointer_set;

static const  int initial_vector_size = 100;
static unsigned int interior_pointer_num_count = 0;


void add_root_set_entry_interior_pointer(void **slot, int offset, Boolean is_pinned)
{
	//check size;
	if( interior_pointer_set.size() == interior_pointer_num_count ) 
	{
		unsigned int size = interior_pointer_num_count == 0 ? initial_vector_size : interior_pointer_set.size()*2;
		interior_pointer_set.resize(size);
	}

	Partial_Reveal_Object* p_obj = (Partial_Reveal_Object*) ((Byte*)*slot - offset);
	assert(p_obj->vt_raw);
	slot_offset_entry* push_back_entry = (slot_offset_entry*)&interior_pointer_set[interior_pointer_num_count++];
	push_back_entry->offset = offset;
	push_back_entry->slot   = slot;
	*slot = p_obj;
	gc_add_root_set_entry((Managed_Object_Handle*)slot, is_pinned);	
}

void update_rootset_interior_pointer()
{
	unsigned int i;
	for( i = 0; i<interior_pointer_num_count; i++)
	{
		slot_offset_entry* entry_traverser = (slot_offset_entry*)&interior_pointer_set[i];
		void** root_slot = entry_traverser->slot;
		Partial_Reveal_Object* root_base = (Partial_Reveal_Object*)*root_slot;//entry_traverser->base;
		unsigned int root_offset = entry_traverser->offset;
		void *new_slot_contents = (void *)((Byte*)root_base + root_offset);	
		*root_slot = new_slot_contents;
	}
	interior_pointer_set.clear();
	assert(interior_pointer_set.size()==0);
	interior_pointer_num_count = 0;
}
