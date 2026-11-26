#ifndef GP100_BOARD_COMPONENTS_H
#define GP100_BOARD_COMPONENTS_H

#include <stddef.h>

typedef enum
{
    kGP100_ComponentNoDigitalInterface,
    kGP100_ComponentI2c,
    kGP100_ComponentSpiDisplay,
} gp100_component_interface_t;

typedef enum
{
    kGP100_DriverNotApplicable,
    kGP100_DriverAvailable,
} gp100_component_driver_t;

typedef enum
{
    kGP100_ConfidenceMedium,
    kGP100_ConfidenceMediumHigh,
    kGP100_ConfidenceHigh,
} gp100_component_confidence_t;

typedef struct
{
    const char *marking;
    const char *part;
    const char *role;
    const char *driver_header;
    gp100_component_interface_t interface_type;
    gp100_component_driver_t driver;
    gp100_component_confidence_t confidence;
} gp100_component_t;

/* Photo-derived inventory. Analogue op-amps and the 74HC04 have no register
 * interface, so kGP100_DriverNotApplicable is intentional, not missing work. */
extern const gp100_component_t g_gp100_components[];
extern const size_t g_gp100_component_count;

#endif
