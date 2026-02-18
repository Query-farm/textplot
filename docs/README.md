# DuckDB Textplot Extension by [Query.Farm](https://query.farm)

The **Textplot** extension, developed by **[Query.Farm](https://query.farm)**, brings beautiful text-based data visualization directly to your SQL queries in DuckDB. Create stunning ASCII/Unicode charts, bar graphs, and density plots without leaving your database environment.

## Use Cases

The Textplot extension is perfect for:
- **Quick data exploration**: Visualize distributions and trends directly in your terminal
- **Dashboard creation**: Add visual elements to text-based reports and dashboards
- **Monitoring and alerting**: Create visual indicators for system metrics and KPIs
- **Data quality checks**: Spot outliers and patterns in your data instantly
- **Command-line analytics**: Build beautiful charts for CLI tools and scripts
- **Documentation**: Include visual data summaries in README files and documentation
- **Embedded analytics**: Add lightweight visualizations to applications without heavy charting libraries

## Installation

**`textplot` is a [DuckDB Community Extension](https://github.com/duckdb/community-extensions).**

You can now use this by using this SQL:

```sql
install textplot from community;
load textplot;
```

## Functions

### `tp_bar(value, ...options)`
Creates horizontal bar charts with customizable styling and colors.

**Basic Usage:**
```sql
-- Simple progress bar (50% filled)
SELECT tp_bar(0.5);
┌──────────────────────┐
│     tp_bar(0.5)      │
│       varchar        │
├──────────────────────┤
│ 🟥🟥🟥🟥🟥⬜⬜⬜⬜⬜ │
└──────────────────────┘

-- Custom width and range
SELECT tp_bar(75, min := 0, max := 100, width := 20);
┌───────────────────────────────────────────────┐
│ tp_bar(75, min := 0, max := 100, width := 20) │
│                    varchar                    │
├───────────────────────────────────────────────┤
│ 🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥🟥⬜⬜⬜⬜⬜      │
└───────────────────────────────────────────────┘
```

**Advanced Options:**
```sql
-- Custom colors and shapes
SELECT tp_bar(0.8, shape := 'circle', on_color := 'green', off_color := 'white') as bar;
┌──────────────────────┐
│         bar          │
│       varchar        │
├──────────────────────┤
│ 🟢🟢🟢🟢🟢🟢🟢🟢⚪⚪ │
└──────────────────────┘

-- Threshold-based coloring
SELECT tp_bar(85,
    min := 0, max := 100,
    thresholds := [
        {'threshold': 90, 'color': 'red'},
        {'threshold': 70, 'color': 'yellow'},
        {'threshold': 0, 'color': 'green'}
    ]
) as bar;
┌──────────────────────┐
│         bar          │
│       varchar        │
├──────────────────────┤
│ 🟨🟨🟨🟨🟨🟨🟨🟨🟨⬜ │
└──────────────────────┘

-- Custom characters (note: "on" and "off" are reserved keywords, so they must be quoted)
SELECT tp_bar(0.7, "on" := '█', "off" := '░', width := 15) as bar;
┌─────────────────┐
│       bar       │
│     varchar     │
├─────────────────┤
│ ███████████░░░░ │
└─────────────────┘

SELECT round(n,4),
tp_bar(n,
width := 14,
shape := 'circle',
off_color :='black',
filled := false,
thresholds := [
  (0.8, 'red'),
  (0.7, 'orange'),
  (0.6, 'yellow'),
  (0.0, 'green')
]) as bar
FROM (select random() as n from generate_series(10));
┌─────────────┬──────────────────────────────┐
│ round(n, 4) │             bar              │
│   double    │           varchar            │
├─────────────┼──────────────────────────────┤
│      0.9889 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫🔴 │
│      0.6242 │ ⚫⚫⚫⚫⚫⚫⚫⚫🟡⚫⚫⚫⚫⚫ │
│      0.6013 │ ⚫⚫⚫⚫⚫⚫⚫🟡⚫⚫⚫⚫⚫⚫ │
│       0.669 │ ⚫⚫⚫⚫⚫⚫⚫⚫🟡⚫⚫⚫⚫⚫ │
│      0.8363 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫🔴⚫⚫ │
│      0.9193 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫🔴⚫ │
│      0.8629 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫🔴⚫⚫ │
│      0.8296 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫🔴⚫⚫ │
│      0.0824 │ 🟢⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.0281 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.2682 │ ⚫⚫⚫🟢⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
├─────────────┴──────────────────────────────┤
│ 11 rows                          2 columns │
└────────────────────────────────────────────┘

SELECT round(n,4),
tp_bar(n,
width := 14,
shape := 'circle',
off_color :='black',
filled := true,
thresholds := [
  (0.8, 'red'),
  (0.7, 'orange'),
  (0.6, 'yellow'),
  (0.0, 'green')
]) as bar
FROM (select random() as n from generate_series(10));
┌─────────────┬──────────────────────────────┐
│ round(n, 4) │             bar              │
│   double    │           varchar            │
├─────────────┼──────────────────────────────┤
│      0.3818 │ 🟢🟢🟢🟢🟢⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.1919 │ 🟢🟢🟢⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.5885 │ 🟢🟢🟢🟢🟢🟢🟢🟢⚫⚫⚫⚫⚫⚫ │
│      0.9558 │ 🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴🔴⚫ │
│      0.4463 │ 🟢🟢🟢🟢🟢🟢⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.6024 │ 🟡🟡🟡🟡🟡🟡🟡🟡⚫⚫⚫⚫⚫⚫ │
│      0.0114 │ ⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
│      0.4993 │ 🟢🟢🟢🟢🟢🟢🟢⚫⚫⚫⚫⚫⚫⚫ │
│      0.6884 │ 🟡🟡🟡🟡🟡🟡🟡🟡🟡🟡⚫⚫⚫⚫ │
│      0.7929 │ 🟠🟠🟠🟠🟠🟠🟠🟠🟠🟠🟠⚫⚫⚫ │
│      0.3507 │ 🟢🟢🟢🟢🟢⚫⚫⚫⚫⚫⚫⚫⚫⚫ │
├─────────────┴──────────────────────────────┤
│ 11 rows                          2 columns │
└────────────────────────────────────────────┘
```

**Parameters:**
- `value`: Numeric value to visualize
- `min`: Minimum value (default: 0)
- `max`: Maximum value (default: 1.0)
- `width`: Bar width in characters (default: 10)
- `shape`: 'square', 'circle', or 'heart' (default: 'square')
- `on_color`/`off_color`: Color names (red, green, blue, yellow, etc.)
- `"on"`/`"off"`: Custom characters for filled/empty portions (must be quoted - reserved keywords)
- `filled`: Boolean, fill all blocks or just the endpoint (default: true)
- `thresholds`: List of threshold objects for conditional coloring

### `tp_density(values, ...options)`
Creates density plots and histograms from arrays of numeric data.

**Basic Usage:**
```sql
-- Simple density plot
SELECT tp_density([1, 2, 3, 4, 5, 4, 3, 2, 1]) as density;
┌──────────────────────┐
│       density        │
│       varchar        │
├──────────────────────┤
│ █    █    █    █   ▒ │
└──────────────────────┘

-- Custom width and style
SELECT tp_density([1, 2, 3, 2, 1], width := 30, style := 'height') as density;
┌────────────────────────────────┐
│            density             │
│            varchar             │
├────────────────────────────────┤
│ █              █             ▄ │
└────────────────────────────────┘
```

**Style Options:**
```sql
-- ASCII style for compatibility
SELECT tp_density([1, 5, 3, 8, 2], style := 'ascii') as density;
┌──────────────────────┐
│       density        │
│       varchar        │
├──────────────────────┤
│ @ @  @     @       @ │
└──────────────────────┘

-- Dot style for subtle visualization
SELECT tp_density([1, 5, 3, 8, 2], style := 'dots') as density;
┌──────────────────────┐
│       density        │
│       varchar        │
├──────────────────────┤
│ ● ●  ●     ●       ● │
└──────────────────────┘

-- Colorful emoji styles
SELECT tp_density([1, 5, 3, 8, 2], style := 'rainbow_circle') as density;
┌──────────────────────────────────────────┐
│                 density                  │
│                 varchar                  │
├──────────────────────────────────────────┤
│ ⚪⚫⚪⚫⚫⚪⚫⚫⚫⚫⚫⚪⚫⚫⚫⚫⚫⚫⚫⚪ │
└──────────────────────────────────────────┘
```

**Parameters:**
- `values`: Array of numeric values
- `width`: Plot width in characters (default: 20)
- `style`: Character set style ('shaded', 'ascii', 'dots', 'height', 'circles', 'safety', 'rainbow_circle', 'rainbow_square', 'moon', 'sparse', 'white')
- `graph_chars`: Custom array of characters for density levels
- `marker`: Character to highlight specific values

**Available Styles:**
- `shaded`: ` ░▒▓█` (default)
- `ascii`: ` .:+#@`
- `dots`: ` .•●`
- `height`: ` ▁▂▃▄▅▆▇█`
- `circles`: `⚫⚪🟡🟠🔴`
- `rainbow_circle`: `⚫🟤🟣🔵🟢🟡🟠🔴⚪`

### `tp_sparkline(values, ...options)`
Creates compact sparkline charts perfect for showing trends in time series data and small multiples.

**Basic Usage:**
```sql
-- Simple absolute value sparkline
SELECT tp_sparkline([1, 3, 2, 5, 4, 6, 2, 1]) as sparkline;
┌──────────────────────┐
│      sparkline       │
│       varchar        │
├──────────────────────┤
│    ==---##***@@---   │
└──────────────────────┘

-- Delta mode showing change direction
SELECT tp_sparkline([100, 105, 102, 108, 95], mode := 'delta') as sparkline;
┌──────────────────────┐
│      sparkline       │
│       varchar        │
├──────────────────────┤
│ ↑↑↑↑↑↓↓↓↓↓↑↑↑↑↑↓↓↓↓↓ │
└──────────────────────┘

-- Trend mode with magnitude
SELECT tp_sparkline([10, 12, 11, 15, 8], mode := 'trend', theme := 'slopes') as sparkline;
┌───────────────────────────┐
│         sparkline         │
│          varchar          │
├───────────────────────────┤
│ /////\\\\\/////\\\\\\\\\\ │
└───────────────────────────┘
```

**Visualization Modes:**

**1. Absolute Mode (default)** - Shows actual values as heights:
```sql
-- Stock prices over time
SELECT tp_sparkline([45.2, 47.1, 46.8, 49.3, 52.1, 48.7], width := 20, theme := 'utf8_blocks') as sparkline;
┌──────────────────────┐
│      sparkline       │
│       varchar        │
├──────────────────────┤
│     ▂▂▂▂▂▂▅▅▅▅███▄▄▄ │
└──────────────────────┘

-- System CPU usage
SELECT tp_sparkline([25, 45, 78, 92, 67, 34], theme := 'ascii_basic', width := 15) as sparkline;
┌─────────────────┐
│    sparkline    │
│     varchar     │
├─────────────────┤
│    --###@@***.. │
└─────────────────┘
```

**2. Delta Mode** - Shows direction of change (up/same/down):
```sql
-- Sales trend directions
SELECT tp_sparkline([1000, 1200, 1150, 1300, 980], mode := 'delta', theme := 'arrows') as sparkline;
┌──────────────────────┐
│      sparkline       │
│       varchar        │
├──────────────────────┤
│ ↑↑↑↑↑↓↓↓↓↓↑↑↑↑↑↓↓↓↓↓ │
└──────────────────────┘

-- Temperature changes
SELECT tp_sparkline([72, 75, 75, 78, 71], mode := 'delta', theme := 'faces') as sparkline;
┌──────────────────────────────────────────┐
│                sparkline                 │
│                 varchar                  │
├──────────────────────────────────────────┤
│ 😊😊😊😊😊😐😐😐😐😐😊😊😊😊😊😞😞😞😞😞 │
└──────────────────────────────────────────┘
```

**3. Trend Mode** - Shows change direction with magnitude:
```sql
-- Market volatility
SELECT tp_sparkline([100, 110, 105, 125, 90], mode := 'trend', theme := 'intensity') as sparkline;
┌───────────────────────────┐
│         sparkline         │
│          varchar          │
├───────────────────────────┤
│ +++++-----+++++---------- │
└───────────────────────────┘

-- Server response times
SELECT tp_sparkline([150, 145, 147, 180, 120], mode := 'trend', theme := 'arrows') as sparkline;
┌──────────────────────┐
│      sparkline       │
│       varchar        │
├──────────────────────┤
│ ↓↓↓↓↓↑↑↑↑↑↑↑↑↑↑⇩⇩⇩⇩⇩ │
└──────────────────────┘

```

**Available Themes by Mode:**

**Absolute Mode Themes:**
- `utf8_blocks`: ` ▁▂▃▄▅▆▇█` (default)
- `ascii_basic`: ` .-=+*#%@`
- `hearts`: ` 🤍🤎❤️💛💚💙💜🖤`
- `faces`: ` 😐🙂😊😃😄😁🤩🤯`

**Delta Mode Themes:**
- `arrows`: `↓→↑` (default)
- `triangles`: `▼◆▲`
- `ascii_arrows`: `v-^`
- `math`: `-=+`
- `faces`: `😞😐😊`
- `thumbs`: `👎👍👍`
- `trends`: `📉➡️📈`
- `simple`: `\\_/`

**Trend Mode Themes:**
- `arrows`: `⇩↓→↑⇧` (default)
- `ascii`: `Vv-^A`
- `slopes`: `\\\\ \\ _ / //`
- `intensity`: `-- - = + ++`
- `faces`: `😭😞😐😊🤩`
- `chart`: `📉📊➡️📊📈`

**Parameters:**
- `values`: Array of numeric values
- `mode`: 'absolute', 'delta', or 'trend' (default: 'absolute')
- `theme`: Theme name (varies by mode, see lists above)
- `width`: Sparkline width in characters (default: 20)

### `tp_qr(value, ...options)`
Creates QR codes with customizable error correction levels and display styles.

**Basic Usage:**
```sql
.mode ascii
-- Simple QR code
SELECT tp_qr('Hello, World!');
select  tp_qr('hello world', ecc := 'high');
⬛⬛⬛⬛⬛⬛⬛⬜⬛⬜⬛⬜⬛⬜⬜⬜⬛⬜⬛⬛⬛⬛⬛⬛⬛
⬛⬜⬜⬜⬜⬜⬛⬜⬛⬛⬜⬜⬜⬛⬛⬛⬜⬜⬛⬜⬜⬜⬜⬜⬛
⬛⬜⬛⬛⬛⬜⬛⬜⬛⬜⬛⬜⬛⬛⬜⬛⬛⬜⬛⬜⬛⬛⬛⬜⬛
⬛⬜⬛⬛⬛⬜⬛⬜⬜⬜⬜⬛⬛⬜⬛⬛⬜⬜⬛⬜⬛⬛⬛⬜⬛
⬛⬜⬛⬛⬛⬜⬛⬜⬜⬛⬜⬜⬜⬛⬛⬜⬛⬜⬛⬜⬛⬛⬛⬜⬛
⬛⬜⬜⬜⬜⬜⬛⬜⬛⬜⬜⬛⬜⬜⬛⬛⬛⬜⬛⬜⬜⬜⬜⬜⬛
⬛⬛⬛⬛⬛⬛⬛⬜⬛⬜⬛⬜⬛⬜⬛⬜⬛⬜⬛⬛⬛⬛⬛⬛⬛
⬜⬜⬜⬜⬜⬜⬜⬜⬛⬛⬜⬜⬛⬜⬜⬛⬜⬜⬜⬜⬜⬜⬜⬜⬜
⬜⬜⬛⬛⬛⬜⬛⬜⬛⬛⬛⬜⬛⬜⬛⬛⬛⬛⬛⬛⬜⬜⬛⬛⬛
⬛⬛⬛⬜⬜⬛⬜⬜⬛⬜⬜⬜⬛⬛⬜⬛⬜⬜⬜⬛⬜⬜⬛⬜⬜
⬛⬜⬜⬜⬛⬜⬛⬛⬛⬛⬜⬜⬜⬜⬛⬛⬜⬛⬜⬜⬛⬛⬜⬛⬛
⬛⬛⬜⬛⬜⬛⬜⬛⬛⬜⬜⬜⬛⬜⬜⬜⬛⬜⬛⬜⬜⬜⬜⬛⬛
⬜⬜⬛⬜⬜⬛⬛⬛⬜⬛⬜⬛⬛⬛⬛⬛⬜⬛⬛⬛⬛⬛⬛⬛⬛
⬛⬜⬛⬛⬛⬜⬜⬜⬛⬜⬜⬛⬛⬜⬜⬛⬛⬜⬜⬛⬜⬜⬛⬜⬜
⬛⬜⬜⬜⬜⬜⬛⬜⬜⬛⬜⬛⬜⬜⬜⬜⬜⬛⬛⬛⬛⬛⬜⬛⬛
⬛⬜⬛⬛⬛⬜⬜⬜⬜⬜⬛⬜⬜⬜⬛⬜⬛⬜⬛⬛⬛⬜⬜⬜⬛
⬛⬜⬛⬜⬜⬛⬛⬛⬜⬜⬜⬛⬛⬜⬛⬜⬛⬛⬛⬛⬛⬛⬛⬜⬜
⬜⬜⬜⬜⬜⬜⬜⬜⬛⬜⬜⬜⬜⬛⬛⬜⬛⬜⬜⬜⬛⬜⬛⬜⬜
⬛⬛⬛⬛⬛⬛⬛⬜⬜⬜⬜⬜⬜⬛⬛⬛⬛⬜⬛⬜⬛⬜⬛⬛⬛
⬛⬜⬜⬜⬜⬜⬛⬜⬜⬜⬜⬛⬜⬜⬜⬛⬛⬜⬜⬜⬛⬛⬜⬛⬜
⬛⬜⬛⬛⬛⬜⬛⬜⬛⬜⬛⬜⬜⬜⬛⬛⬛⬛⬛⬛⬛⬛⬛⬜⬜
⬛⬜⬛⬛⬛⬜⬛⬜⬛⬛⬜⬜⬜⬛⬛⬜⬜⬜⬛⬜⬛⬛⬜⬜⬛
⬛⬜⬛⬛⬛⬜⬛⬜⬛⬛⬜⬛⬛⬛⬛⬛⬜⬜⬛⬛⬜⬛⬜⬜⬛
⬛⬜⬜⬜⬜⬜⬛⬜⬜⬛⬛⬛⬜⬜⬛⬛⬜⬛⬜⬛⬛⬜⬜⬜⬛

```

**Advanced Options:**
```sql
-- Custom colors and shapes (note: "on" and "off" are reserved keywords, so they must be quoted)
SELECT tp_qr('https://query.farm', ecc := 'high', "on" := '🟡', "off" := '⚫');
🟡🟡🟡🟡🟡🟡🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫⚫⚫🟡⚫🟡⚫🟡🟡🟡🟡🟡🟡🟡
🟡⚫⚫⚫⚫⚫🟡⚫🟡⚫⚫⚫🟡⚫🟡⚫🟡⚫⚫🟡⚫⚫🟡⚫⚫⚫⚫⚫🟡
🟡⚫🟡🟡🟡⚫🟡⚫🟡🟡🟡🟡🟡🟡⚫🟡🟡⚫⚫🟡🟡⚫🟡⚫🟡🟡🟡⚫🟡
🟡⚫🟡🟡🟡⚫🟡⚫⚫⚫⚫🟡🟡🟡⚫⚫🟡🟡⚫🟡⚫⚫🟡⚫🟡🟡🟡⚫🟡
🟡⚫🟡🟡🟡⚫🟡⚫⚫⚫🟡🟡🟡🟡🟡🟡⚫⚫⚫🟡🟡⚫🟡⚫🟡🟡🟡⚫🟡
🟡⚫⚫⚫⚫⚫🟡⚫🟡⚫⚫🟡⚫⚫🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫⚫⚫⚫⚫🟡
🟡🟡🟡🟡🟡🟡🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫🟡⚫🟡🟡🟡🟡🟡🟡🟡
⚫⚫⚫⚫⚫⚫⚫⚫🟡🟡⚫🟡⚫🟡🟡🟡🟡⚫⚫⚫🟡⚫⚫⚫⚫⚫⚫⚫⚫
⚫⚫🟡🟡🟡⚫🟡⚫🟡🟡🟡🟡⚫⚫🟡🟡🟡⚫🟡⚫🟡🟡🟡🟡⚫⚫🟡🟡🟡
🟡🟡🟡🟡⚫🟡⚫🟡⚫🟡🟡🟡⚫🟡⚫🟡⚫⚫⚫⚫⚫🟡⚫⚫🟡🟡🟡⚫🟡
🟡🟡🟡⚫⚫🟡🟡🟡🟡⚫⚫🟡⚫⚫🟡⚫🟡🟡🟡⚫🟡🟡🟡🟡🟡⚫🟡⚫⚫
🟡⚫⚫⚫⚫⚫⚫⚫🟡🟡⚫🟡⚫🟡⚫⚫⚫⚫⚫⚫⚫🟡🟡🟡⚫🟡⚫🟡⚫
⚫🟡🟡🟡🟡🟡🟡⚫⚫⚫⚫🟡🟡🟡🟡🟡⚫⚫🟡⚫🟡🟡⚫🟡⚫⚫🟡🟡🟡
🟡⚫🟡⚫🟡🟡⚫⚫⚫⚫⚫⚫🟡🟡🟡⚫🟡🟡⚫🟡🟡🟡🟡⚫🟡🟡⚫🟡🟡
⚫⚫⚫🟡⚫⚫🟡⚫🟡🟡⚫🟡🟡⚫⚫⚫🟡⚫⚫⚫🟡⚫🟡🟡⚫⚫⚫🟡⚫
⚫⚫🟡⚫⚫🟡⚫⚫🟡⚫🟡🟡⚫⚫🟡🟡🟡⚫🟡⚫⚫🟡🟡⚫🟡🟡⚫⚫⚫
⚫🟡⚫⚫🟡🟡🟡🟡⚫⚫⚫⚫🟡⚫⚫⚫⚫🟡🟡⚫⚫⚫🟡⚫⚫🟡🟡🟡🟡
🟡🟡🟡⚫⚫🟡⚫🟡🟡⚫🟡⚫⚫⚫🟡🟡⚫🟡⚫⚫🟡🟡⚫🟡🟡🟡⚫🟡🟡
🟡⚫⚫🟡⚫🟡🟡🟡⚫⚫🟡🟡⚫⚫⚫🟡🟡⚫🟡🟡⚫⚫🟡⚫⚫⚫🟡⚫⚫
🟡⚫🟡⚫⚫⚫⚫⚫🟡🟡🟡🟡🟡🟡🟡⚫⚫⚫⚫🟡🟡⚫🟡⚫🟡🟡⚫🟡🟡
🟡⚫🟡🟡⚫🟡🟡⚫⚫⚫🟡⚫🟡🟡⚫⚫⚫⚫⚫🟡🟡🟡🟡🟡🟡⚫🟡🟡⚫
⚫⚫⚫⚫⚫⚫⚫⚫🟡🟡⚫⚫🟡⚫⚫⚫🟡🟡🟡🟡🟡⚫⚫⚫🟡🟡⚫⚫🟡
🟡🟡🟡🟡🟡🟡🟡⚫⚫⚫⚫⚫⚫⚫🟡⚫⚫⚫🟡🟡🟡⚫🟡⚫🟡⚫⚫⚫⚫
🟡⚫⚫⚫⚫⚫🟡⚫⚫🟡⚫⚫🟡⚫🟡🟡🟡🟡⚫🟡🟡⚫⚫⚫🟡🟡⚫🟡🟡
🟡⚫🟡🟡🟡⚫🟡⚫🟡🟡🟡🟡⚫🟡⚫🟡⚫🟡⚫⚫🟡🟡🟡🟡🟡🟡🟡🟡⚫
🟡⚫🟡🟡🟡⚫🟡⚫🟡🟡⚫🟡⚫🟡🟡⚫🟡🟡🟡🟡🟡🟡⚫🟡⚫⚫⚫⚫⚫
🟡⚫🟡🟡🟡⚫🟡⚫🟡🟡⚫⚫⚫⚫🟡🟡⚫⚫⚫🟡🟡🟡🟡🟡🟡🟡⚫🟡⚫
🟡⚫⚫⚫⚫⚫🟡⚫⚫⚫⚫⚫🟡🟡⚫🟡🟡⚫🟡🟡⚫🟡⚫⚫🟡🟡⚫🟡⚫
🟡🟡🟡🟡🟡🟡🟡⚫⚫🟡🟡⚫🟡🟡⚫⚫🟡🟡⚫⚫⚫⚫⚫⚫🟡🟡🟡⚫⚫

```

**Parameters:**
- `value`: String value to encode in the QR code.
- `ecc`: Error correction level ('low', 'medium', 'quartile', 'high', default: 'low')
- `"on"`: Character for filled modules (default: '⬛') - must be quoted (reserved keyword)
- `"off"`: Character for empty modules (default: '⬜') - must be quoted (reserved keyword)


## Tips and Best Practices

1. **Choose appropriate widths**: Longer bars (width 20-30) work well for dashboards, shorter bars (width 10-15) for compact reports
2. **Use thresholds for status indicators**: Perfect for showing health, performance, or risk levels
3. **Combine with regular metrics**: Text plots complement, don't replace, numeric values
4. **Consider your audience**: ASCII styles work everywhere, emoji styles are more visually appealing but require Unicode support
5. **Leverage density plots for distributions**: Great for showing data patterns, outliers, and distributions

## Contributing

The Textplot extension is open source and developed by [Query.Farm](https://query.farm). Contributions are welcome!

## License

[MIT License](LICENSE)
