<p align="center">
  <a href="https://query.farm">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://query.farm/media-kit/logo/wordmark-dark.svg">
      <img alt="Query.Farm" src="https://query.farm/media-kit/logo/wordmark-light.svg" height="64">
    </picture>
  </a>
</p>

# DuckDB Textplot Extension by [Query.Farm](https://query.farm)

[![DuckDB](https://img.shields.io/badge/DuckDB-community_extension-fdf1e0?logo=duckdb&logoColor=fff000)](https://duckdb.org/community_extensions/extensions/textplot.html)
[![v1.5 build](https://github.com/Query-farm/textplot/actions/workflows/MainDistributionPipeline.yml/badge.svg?branch=v1.5)](https://github.com/Query-farm/textplot/actions/workflows/MainDistributionPipeline.yml?query=branch%3Av1.5)

The **Textplot** extension, developed by **[Query.Farm](https://query.farm)**, brings beautiful text-based data visualization directly to your SQL queries in DuckDB. Create stunning ASCII/Unicode charts, bar graphs, and density plots without leaving your database environment.

## Documentation

Full documentation, including installation, usage, the function reference, and cookbook examples, is available at:

**[https://query.farm/products/extensions/textplot](https://query.farm/products/extensions/textplot)**

## Installation

```sql
install textplot from community;
load textplot;
```
