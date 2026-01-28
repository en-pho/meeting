"""
Simple Camera Simulator (Teaching Version)
------------------------------------------
This script simulates how camera parameters affect an image.

You ONLY need to change the three parameters below:
    - f_number (aperture)
    - exposure_time (shutter speed)
    - ISO

Everything else should be left unchanged.
"""

# ============================================================
# === 1. CAMERA PARAMETERS (CHANGE ONLY THESE THREE) =========
# ============================================================

f_number = 5.0        # Aperture: smaller = brighter + more blur
exposure_time = 0.2  # Relative shutter time (unitless)
ISO = 500             # Sensor gain: higher = noisier

# ============================================================
# === 2. IMPORTS =============================================
# ============================================================

import numpy as np
import matplotlib.pyplot as plt
from scipy.ndimage import gaussian_filter
from skimage import io, img_as_float
from skimage.util import random_noise

# ============================================================
# === 3. LOAD IMAGE (SCENE BEING PHOTOGRAPHED) ================
# ============================================================

image = img_as_float(io.imread("cell.jpg", as_gray=True))

# ============================================================
# === 4. CAMERA MODEL ========================================
# ============================================================

# --- Aperture effect: brightness + depth of field (blur)
blur_sigma = max(0.2, 6.0 / f_number)
image_aperture = gaussian_filter(image, sigma=blur_sigma)

aperture_gain = (2.8 / f_number) ** 2
image_aperture *= aperture_gain


# --- Exposure time: linear brightness scaling
image_exposed = image_aperture * exposure_time


# --- ISO: noise amplification (does NOT add information)
noise_variance = ISO / 20000
image_noisy = random_noise(image_exposed, var=noise_variance)


# --- Clip to valid image range
final_image = np.clip(image_noisy, 0, 1)

# ============================================================
# === 5. DISPLAY RESULTS =====================================
# ============================================================

plt.figure(figsize=(10, 4))

plt.subplot(1, 2, 1)
plt.imshow(image, cmap="gray")
plt.title("Original Scene")
plt.axis("off")

plt.subplot(1, 2, 2)
plt.imshow(final_image, cmap="gray")
plt.title(f"f/{f_number}, t={exposure_time}, ISO={ISO}")
plt.axis("off")

plt.tight_layout()
plt.show()
