/**
 * PixelForge - Master Client Application
 * Full-stack vanilla ES6 implementation connecting the SaaS frontend 
 * to the custom native C++ Winsock2 backend engine.
 */

// Application Global State
const state = {
  theme: 'dark',
  currentTab: 'home',
  mode: 'lossless', // 'lossless' | 'lossy'
  quality: 75,
  sourceImageB64: null,
  sourceFilename: 'image.bmp',
  sourceFileSize: 0,
  sourceWidth: 0,
  sourceHeight: 0,
  compressionResult: null,
  isProcessing: false
};

// =============================================================================
// 1. Theme Management (Light / Dark Mode with Persistence)
// =============================================================================

function initTheme() {
  const savedTheme = localStorage.getItem('pixelforge-theme');
  if (savedTheme) {
    setTheme(savedTheme);
  } else {
    // Respect OS preference on first visit
    const prefersLight = window.matchMedia('(prefers-color-scheme: light)').matches;
    setTheme(prefersLight ? 'light' : 'dark');
  }

  const toggleBtn = document.getElementById('themeToggleBtn');
  if (toggleBtn) {
    toggleBtn.addEventListener('click', () => {
      const newTheme = state.theme === 'dark' ? 'light' : 'dark';
      setTheme(newTheme);
    });
  }
}

function setTheme(theme) {
  state.theme = theme;
  document.documentElement.setAttribute('data-theme', theme);
  localStorage.setItem('pixelforge-theme', theme);
}

// =============================================================================
// 2. Navigation & View Routing
// =============================================================================

function navigateTo(tabId, updateHash = true) {
  state.currentTab = tabId;

  // Update navbar links
  document.querySelectorAll('.nav-link').forEach(btn => {
    btn.classList.toggle('active', btn.dataset.tab === tabId);
  });

  // Switch active page section
  document.querySelectorAll('.page-view').forEach(view => {
    view.classList.toggle('active', view.id === `page-${tabId}`);
  });

  // Close mobile navigation menu if open
  const navMenu = document.getElementById('navMenu');
  if (navMenu && navMenu.classList.contains('open')) {
    navMenu.classList.remove('open');
  }

  // Update URL hash without causing unwanted scroll jumps
  if (updateHash && window.location.hash !== `#${tabId}`) {
    history.pushState(null, '', `#${tabId}`);
  }

  // Auto-scroll to top smoothly
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

function handleHashRoute() {
  const hash = window.location.hash.replace('#', '');
  const validTabs = ['home', 'workspace'];
  if (validTabs.includes(hash)) {
    navigateTo(hash, false);
  }
}

window.addEventListener('popstate', handleHashRoute);
window.addEventListener('hashchange', handleHashRoute);

function selectModeAndGo(mode) {
  setMode(mode);
  navigateTo('workspace');
}

// Mobile Hamburger Toggle
const mobileNavToggle = document.getElementById('mobileNavToggle');
if (mobileNavToggle) {
  mobileNavToggle.addEventListener('click', () => {
    document.getElementById('navMenu').classList.toggle('open');
  });
}

// =============================================================================
// 3. Mode & Quality Slider Controls
// =============================================================================

function setMode(mode) {
  state.mode = mode;

  const btnLossless = document.getElementById('btnWorkspaceLossless');
  const btnLossy = document.getElementById('btnWorkspaceLossy');
  const losslessPanel = document.getElementById('losslessDetailsPanel');
  const lossyPanel = document.getElementById('lossyDetailsPanel');

  if (btnLossless && btnLossy) {
    btnLossless.classList.toggle('active', mode === 'lossless');
    btnLossy.classList.toggle('active', mode === 'lossy');
  }

  if (losslessPanel && lossyPanel) {
    losslessPanel.classList.toggle('hidden', mode !== 'lossless');
    lossyPanel.classList.toggle('hidden', mode !== 'lossy');
  }

  const compressBtn = document.getElementById('executeCompressBtn');
  if (compressBtn) {
    if (mode === 'lossless') {
      compressBtn.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="vertical-align:middle;margin-right:6px"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"></path><polyline points="9 12 11 14 15 10"></polyline></svg><span>Compress Losslessly</span>';
    } else {
      compressBtn.innerHTML = '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor" stroke="none" style="vertical-align:middle;margin-right:6px"><polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"></polygon></svg><span>Compress with Lossy DCT</span>';
    }
  }
}

function onQualitySliderChange(val) {
  state.quality = parseInt(val, 10);
  const display = document.getElementById('sliderValueDisplay');
  if (display) {
    display.textContent = val;
  }
}

// =============================================================================
// 4. File Drag & Drop, Upload Validation & Previews
// =============================================================================

function initUploadHandlers() {
  setupDropZone('homeDropZone', 'homeFileInput');
  setupDropZone('workspaceDropZone', 'workspaceFileInput');
}

function setupDropZone(dropZoneId, fileInputId) {
  const dropZone = document.getElementById(dropZoneId);
  const fileInput = document.getElementById(fileInputId);
  if (!dropZone || !fileInput) return;

  dropZone.addEventListener('click', () => fileInput.click());

  dropZone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropZone.classList.add('drag-active');
  });

  dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('drag-active');
  });

  dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('drag-active');
    if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      handleFileSelected(e.dataTransfer.files[0]);
    }
  });

  fileInput.addEventListener('change', (e) => {
    if (e.target.files && e.target.files.length > 0) {
      handleFileSelected(e.target.files[0]);
    }
  });
}

function triggerFileInput() {
  const fileInput = document.getElementById('workspaceFileInput');
  if (fileInput) fileInput.click();
}

function handleFileSelected(file) {
  if (!file) return;

  // Strict BMP validation (as mandated by project spec)
  if (!file.name.toLowerCase().endsWith('.bmp')) {
    showToast('Invalid format. Please select an uncompressed 24-bit .BMP image.', 'error');
    return;
  }

  state.sourceFilename = file.name;
  state.sourceFileSize = file.size;

  const reader = new FileReader();
  reader.onload = function(e) {
    const dataUrl = e.target.result;
    state.sourceImageB64 = dataUrl.split(',')[1];

    // Load into an Image object to read natural dimensions
    const imgObj = new Image();
    imgObj.onload = function() {
      state.sourceWidth = imgObj.naturalWidth;
      state.sourceHeight = imgObj.naturalHeight;

      // Update Workspace UI elements
      document.getElementById('loadedFileName').textContent = state.sourceFilename;
      document.getElementById('loadedFileDim').textContent = `${state.sourceWidth} x ${state.sourceHeight} px`;
      document.getElementById('loadedFileSize').textContent = formatBytes(state.sourceFileSize);

      document.getElementById('fileDetailsPill').classList.remove('hidden');
      document.getElementById('replaceBtn').classList.remove('hidden');

      // Update Image Preview
      const previewImg = document.getElementById('mainInputImage');
      previewImg.src = dataUrl;

      document.getElementById('emptyWorkspaceView').classList.add('hidden');
      document.getElementById('resultsWorkspaceView').classList.add('hidden');
      document.getElementById('loadedWorkspaceView').classList.remove('hidden');

      // Enable compression button
      document.getElementById('executeCompressBtn').disabled = false;

      // If uploaded from homepage, navigate directly to workspace
      if (state.currentTab !== 'workspace') {
        navigateTo('workspace');
      }

      showToast(`Loaded ${state.sourceFilename} (${state.sourceWidth}x${state.sourceHeight})`);
    };
    imgObj.src = dataUrl;
  };

  reader.readAsDataURL(file);
}

function clearLoadedImage() {
  state.sourceImageB64 = null;
  state.sourceFileSize = 0;
  state.sourceFilename = 'image.bmp';

  document.getElementById('fileDetailsPill').classList.add('hidden');
  document.getElementById('replaceBtn').classList.add('hidden');
  document.getElementById('loadedWorkspaceView').classList.add('hidden');
  document.getElementById('resultsWorkspaceView').classList.add('hidden');
  document.getElementById('emptyWorkspaceView').classList.remove('hidden');
  document.getElementById('executeCompressBtn').disabled = true;

  // Reset file inputs
  const homeInput = document.getElementById('homeFileInput');
  const wsInput = document.getElementById('workspaceFileInput');
  if (homeInput) homeInput.value = '';
  if (wsInput) wsInput.value = '';
}

// Pre-generated Sample Image Loader
async function loadSampleImage(filename) {
  showLoading(`Loading pre-built pattern: ${filename}...`);
  try {
    const res = await fetch(`/samples/input/${filename}`);
    if (!res.ok) throw new Error(`Sample ${filename} not found on server.`);
    const blob = await res.blob();
    const file = new File([blob], filename, { type: 'image/bmp' });
    handleFileSelected(file);
  } catch (err) {
    showToast(`Failed to load sample: ${err.message}`, 'error');
  } finally {
    hideLoading();
  }
}

// =============================================================================
// 5. Compression Execution & Real-Time API Dispatch
// =============================================================================

async function executeCompression() {
  if (!state.sourceImageB64 || state.isProcessing) return;

  state.isProcessing = true;
  document.getElementById('executeCompressBtn').disabled = true;
  showLoading(`Running custom ${state.mode === 'lossless' ? 'Lossless' : 'Lossy'} C++ engine...`);

  const payload = {
    mode: state.mode,
    quality: state.quality,
    image_b64: state.sourceImageB64
  };

  try {
    const response = await fetch('/api/compress', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    if (!response.ok) {
      throw new Error(`Server returned HTTP ${response.status}`);
    }

    const data = await response.json();
    if (!data.success) {
      throw new Error(data.error || 'Compression failed in engine.');
    }

    state.compressionResult = data;
    renderCompressionResults(data);
    showToast('Compression completed successfully!');
  } catch (err) {
    showToast(`Engine Error: ${err.message}`, 'error');
  } finally {
    state.isProcessing = false;
    document.getElementById('executeCompressBtn').disabled = false;
    hideLoading();
  }
}

function renderCompressionResults(res) {
  // Hide single image preview, show Results & Splitter container
  document.getElementById('loadedWorkspaceView').classList.add('hidden');
  document.getElementById('resultsWorkspaceView').classList.remove('hidden');

  // Populate KPIs
  document.getElementById('kpiOriginalSize').textContent = formatBytes(res.original_bytes);
  document.getElementById('kpiCompressedSize').textContent = formatBytes(res.compressed_bytes);
  document.getElementById('kpiReduction').textContent = `${parseFloat(res.percentage_reduction).toFixed(1)}%`;
  document.getElementById('kpiRatio').textContent = `${parseFloat(res.compression_ratio).toFixed(2)}x`;
  document.getElementById('kpiTime').textContent = `${parseFloat(res.compression_time_ms).toFixed(2)} ms`;

  // Format Badge
  document.getElementById('resultFormatBadge').textContent = res.filename_ext;
  document.getElementById('downloadExtLabel').textContent = res.filename_ext;

  // Fidelity / Verification cell
  const fidelityCell = document.getElementById('kpiFidelityCell');
  const fidelityText = document.getElementById('kpiFidelity');

  if (res.mode === 'lossy') {
    fidelityCell.querySelector('.kpi-label').textContent = 'Fidelity (PSNR / MSE)';
    fidelityText.innerHTML = `${parseFloat(res.psnr).toFixed(2)} dB <span style="font-size:0.75rem; color:var(--text-sub)">(MSE: ${parseFloat(res.mse).toFixed(2)})</span>`;
  } else {
    fidelityCell.querySelector('.kpi-label').textContent = 'Verification';
    fidelityText.innerHTML = res.verified_exact 
      ? '<span style="color:var(--status-success)">100% Exact Match</span>' 
      : '<span style="color:var(--status-error)">Mismatch</span>';
  }

  // Save state for preview toggle
  state.lastResult = res;

  // Setup Before-and-After Images
  const origImg = document.getElementById('comparisonOrigImg');
  const reconImg = document.getElementById('comparisonReconImg');

  origImg.src = `data:image/bmp;base64,${state.sourceImageB64}`;
  reconImg.src = `data:image/bmp;base64,${res.reconstructed_bmp_b64}`;

  // Default comparison to Engine output
  document.getElementById('previewEngineExt').textContent = res.filename_ext;
  document.getElementById('sliderRightTag').textContent = `Engine Output (${res.filename_ext})`;
  document.getElementById('segBtnEngine').classList.add('active');
  document.getElementById('segBtnBmp').classList.remove('active');

  // Populate Visual Format Showcase Gallery
  const baseName = state.sourceFilename.replace(/\.[^/.]+$/, "");
  const rleBmpSrc = res.rle_preview_bmp_b64 || res.compressed_rle_bmp_b64 || res.reconstructed_bmp_b64;

  // 1. Original
  document.getElementById('showcaseOrigImg').src = `data:image/bmp;base64,${state.sourceImageB64}`;
  document.getElementById('showcaseOrigSize').textContent = formatBytes(res.original_bytes);

  // 2. Compressed .BMP (RLE8)
  document.getElementById('showcaseBmpImg').src = `data:image/bmp;base64,${rleBmpSrc}`;
  const rleBytes = res.rle_bmp_bytes || res.compressed_bytes;
  document.getElementById('showcaseBmpSize').textContent = formatBytes(rleBytes);
  const rleReduction = res.original_bytes > 0 ? ((1.0 - (rleBytes / res.original_bytes)) * 100.0).toFixed(1) : '0';
  document.getElementById('showcaseBmpReduction').textContent = `${rleReduction}% reduction`;
  const showcaseBmpDl = document.getElementById('showcaseBmpDownloadBtn');
  showcaseBmpDl.href = `data:image/bmp;base64,${res.compressed_rle_bmp_b64 || rleBmpSrc}`;
  showcaseBmpDl.download = `${baseName}_compressed.bmp`;

  // 3. Engine Output
  document.getElementById('showcaseEngineImg').src = `data:image/bmp;base64,${res.reconstructed_bmp_b64}`;
  document.getElementById('showcaseEngineExt').textContent = res.filename_ext;
  document.getElementById('showcaseEngineSize').textContent = formatBytes(res.compressed_bytes);
  document.getElementById('showcaseEngineReduction').textContent = `${parseFloat(res.percentage_reduction).toFixed(1)}% reduction`;
  document.getElementById('showcaseEnginePSNR').textContent = res.mode === 'lossy' 
    ? `${parseFloat(res.psnr).toFixed(2)} dB (PSNR)` 
    : 'Exact 100% Match';
  const showcaseEngineDl = document.getElementById('showcaseEngineDownloadBtn');
  showcaseEngineDl.href = `data:application/octet-stream;base64,${res.compressed_b64}`;
  showcaseEngineDl.download = `${baseName}_archive${res.filename_ext}`;

  // Reset comparison slider to 50%
  resetSliderToCenter();

  // Setup Download Links in Bottom Bar
  
  // 1. Engine Archive (.pfl / .pfz)
  const compDownloadBtn = document.getElementById('downloadCompressedFileBtn');
  compDownloadBtn.href = `data:application/octet-stream;base64,${res.compressed_b64}`;
  compDownloadBtn.download = `${baseName}_archive${res.filename_ext}`;
  document.getElementById('downloadCompSizeLabel').textContent = formatBytes(res.compressed_bytes);

  // 2. Compressed 8-Bit RLE BMP (.bmp) - Genuinely smaller on disk
  const rleDownloadBtn = document.getElementById('downloadRleBmpBtn');
  if (res.compressed_rle_bmp_b64) {
    rleDownloadBtn.href = `data:image/bmp;base64,${res.compressed_rle_bmp_b64}`;
    rleDownloadBtn.download = `${baseName}_compressed.bmp`;
    document.getElementById('downloadRleBmpSizeLabel').textContent = formatBytes(res.rle_bmp_bytes);
  } else {
    rleDownloadBtn.href = `data:image/bmp;base64,${res.reconstructed_bmp_b64}`;
    rleDownloadBtn.download = `${baseName}_compressed.bmp`;
    document.getElementById('downloadRleBmpSizeLabel').textContent = formatBytes(res.original_bytes);
  }

  // 3. Raw 24-Bit Reconstructed BMP (.bmp) (if present)
  const reconDownloadBtn = document.getElementById('downloadReconBmpBtn');
  if (reconDownloadBtn) {
    reconDownloadBtn.href = `data:image/bmp;base64,${res.reconstructed_bmp_b64}`;
    reconDownloadBtn.download = `${baseName}_raw_24bit.bmp`;
    const rawLabel = document.getElementById('downloadRawBmpSizeLabel');
    if (rawLabel) rawLabel.textContent = formatBytes(res.original_bytes);
  }
}

function switchPreviewMode(mode) {
  if (!state.lastResult) return;
  const res = state.lastResult;
  const reconImg = document.getElementById('comparisonReconImg');
  const tagRight = document.getElementById('sliderRightTag');
  const btnEngine = document.getElementById('segBtnEngine');
  const btnBmp = document.getElementById('segBtnBmp');

  if (mode === 'bmp') {
    btnBmp.classList.add('active');
    btnEngine.classList.remove('active');
    const bmpSrc = res.rle_preview_bmp_b64 || res.compressed_rle_bmp_b64 || res.reconstructed_bmp_b64;
    reconImg.src = `data:image/bmp;base64,${bmpSrc}`;
    tagRight.textContent = 'Compressed .BMP (8-Bit RLE)';
    showToast('Slider updated: Showing exact Compressed .BMP (Windows format)', 'info');
  } else {
    btnEngine.classList.add('active');
    btnBmp.classList.remove('active');
    reconImg.src = `data:image/bmp;base64,${res.reconstructed_bmp_b64}`;
    tagRight.textContent = `Engine Output (${res.filename_ext})`;
    showToast(`Slider updated: Showing Custom Engine output (${res.filename_ext})`, 'info');
  }
}

function resetToWorkspace() {
  document.getElementById('resultsWorkspaceView').classList.add('hidden');
  document.getElementById('loadedWorkspaceView').classList.remove('hidden');
}

// =============================================================================
// 6. Interactive Before-and-After Image Comparison Slider
// =============================================================================

function initComparisonSlider() {
  const wrapper = document.getElementById('imageComparisonWrapper');
  const divider = document.getElementById('sliderDivider');
  const overlay = document.getElementById('comparisonOverlay');
  if (!wrapper || !divider || !overlay) return;

  let isDragging = false;

  function setSliderPosition(x) {
    const rect = wrapper.getBoundingClientRect();
    let percentage = ((x - rect.left) / rect.width) * 100;
    percentage = Math.max(0, Math.min(100, percentage));

    divider.style.left = `${percentage}%`;
    overlay.style.width = `${percentage}%`;
  }

  // Mouse Listeners
  wrapper.addEventListener('mousedown', (e) => {
    isDragging = true;
    setSliderPosition(e.clientX);
  });

  window.addEventListener('mousemove', (e) => {
    if (!isDragging) return;
    setSliderPosition(e.clientX);
  });

  window.addEventListener('mouseup', () => {
    isDragging = false;
  });

  // Touch Listeners for Mobile
  wrapper.addEventListener('touchstart', (e) => {
    isDragging = true;
    if (e.touches && e.touches[0]) {
      setSliderPosition(e.touches[0].clientX);
    }
  });

  window.addEventListener('touchmove', (e) => {
    if (!isDragging) return;
    if (e.touches && e.touches[0]) {
      setSliderPosition(e.touches[0].clientX);
    }
  });

  window.addEventListener('touchend', () => {
    isDragging = false;
  });
}

function resetSliderToCenter() {
  const divider = document.getElementById('sliderDivider');
  const overlay = document.getElementById('comparisonOverlay');
  if (divider && overlay) {
    divider.style.left = '50%';
    overlay.style.width = '50%';
  }
}

// =============================================================================
// 7. Decompress Tool (Drop .pfl or .pfz directly)
// =============================================================================

function handleDecompressFile(event) {
  const file = event.target.files[0];
  if (!file) return;

  showLoading(`Decompressing container file: ${file.name}...`);
  const reader = new FileReader();

  reader.onload = async function(e) {
    const b64 = e.target.result.split(',')[1];
    try {
      const response = await fetch('/api/decompress', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ data_b64: b64 })
      });

      const res = await response.json();
      if (!res.success) throw new Error(res.error || 'Decompression failed.');

      // Feed reconstructed image into display
      state.sourceImageB64 = res.reconstructed_bmp_b64;
      state.sourceFilename = file.name;
      state.sourceFileSize = file.size;

      renderCompressionResults({
        mode: res.type,
        original_bytes: file.size,
        compressed_bytes: file.size,
        compression_ratio: 1.0,
        percentage_reduction: 0.0,
        compression_time_ms: res.decompression_time_ms,
        filename_ext: file.name.endsWith('.pfl') ? '.pfl' : '.pfz',
        psnr: 999.99,
        mse: 0.0,
        verified_exact: res.verified_exact,
        reconstructed_bmp_b64: res.reconstructed_bmp_b64,
        compressed_b64: b64
      });

      navigateTo('workspace');
      showToast(`Decompressed ${file.name} successfully!`);
    } catch (err) {
      showToast(`Decompression Error: ${err.message}`, 'error');
    } finally {
      hideLoading();
    }
  };

  reader.readAsDataURL(file);
}

// =============================================================================
// 8. Algorithm Lab Live Matrix Visualizer
// =============================================================================

function switchLabTab(tab) {
  const isLossy = tab === 'lossy';
  document.getElementById('labTabLossyBtn').classList.toggle('active', isLossy);
  document.getElementById('labTabLosslessBtn').classList.toggle('active', !isLossy);
  document.getElementById('labLossyContent').classList.toggle('active', isLossy);
  document.getElementById('labLosslessContent').classList.toggle('active', !isLossy);
}

async function fetchAlgorithmDemo() {
  try {
    const res = await fetch('/api/algorithm-demo');
    if (!res.ok) return;
    const data = await res.json();

    render8x8Matrix('labSpatialMatrix', data.spatial_block);
    render8x8Matrix('labDctMatrix', data.dct_coefficients);
    render8x8Matrix('labQuantMatrix', data.quantized_coefficients);

    const zzEl = document.getElementById('labZigzagOutput');
    if (zzEl && data.zigzag) {
      zzEl.textContent = `[ ${data.zigzag.join(', ')} ]`;
    }
  } catch (err) {
    console.error('Failed to load algorithm demo matrix:', err);
  }
}

function render8x8Matrix(containerId, matrix) {
  const el = document.getElementById(containerId);
  if (!el || !matrix) return;

  el.innerHTML = '';
  for (let r = 0; r < 8; ++r) {
    for (let c = 0; c < 8; ++c) {
      const cell = document.createElement('div');
      cell.textContent = matrix[r][c];
      el.appendChild(cell);
    }
  }
}

// =============================================================================
// 9. Toast Notifications & Loading Overlay
// =============================================================================

let toastTimeout = null;

function showToast(msg, type = 'info') {
  const toast = document.getElementById('toastNotification');
  const msgEl = document.getElementById('toastMessage');
  if (!toast || !msgEl) return;

  msgEl.textContent = msg;
  toast.style.borderLeftColor = type === 'error' ? 'var(--status-error)' : 'var(--accent-primary)';
  toast.classList.remove('hidden');

  if (toastTimeout) clearTimeout(toastTimeout);
  toastTimeout = setTimeout(() => {
    toast.classList.add('hidden');
  }, 4000);
}

function showLoading(msg) {
  const overlay = document.getElementById('globalLoadingOverlay');
  const text = document.getElementById('loadingStatusText');
  if (overlay && text) {
    text.textContent = msg;
    overlay.classList.remove('hidden');
  }
}

function hideLoading() {
  const overlay = document.getElementById('globalLoadingOverlay');
  if (overlay) {
    overlay.classList.add('hidden');
  }
}

function formatBytes(bytes) {
  if (!bytes || bytes === 0) return '0 Bytes';
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

// =============================================================================
// 10. Bootstrap Application
// =============================================================================

document.addEventListener('DOMContentLoaded', () => {
  initTheme();
  initUploadHandlers();
  initComparisonSlider();
  handleHashRoute();
});
