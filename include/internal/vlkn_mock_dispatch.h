#ifndef VLKN_MOCK_DISPATCH_H
#define VLKN_MOCK_DISPATCH_H

#include "vlkn_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

void vlkn_mock_dispatch_init(VlknDispatchTable *table);

/*
 * Mock-backend test hooks. Only the mock entry points consult this state;
 * the real Vulkan loader path never touches it. Tests use these to inject
 * Vulkan failures and audit live mock objects, so error paths can prove
 * they neither leak resources nor proceed on invalid handles.
 */
void svga3_mock_reset_hooks(void);
void svga3_mock_fail_create_image_view(int count);
void svga3_mock_fail_bind_image_memory(int count);
void svga3_mock_set_image_memory_type_bits(uint32_t bits);
void svga3_mock_clear_image_memory_type_bits(void);
long svga3_mock_live_images(void);
long svga3_mock_live_memory(void);
long svga3_mock_live_views(void);
long svga3_mock_allocate_calls(void);
int  svga3_mock_saw_invalid_alloc_type(void);
long svga3_mock_unbound_image_uses(void);

#ifdef __cplusplus
}
#endif
#endif
