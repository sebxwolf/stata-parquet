// Stata function: Low-level read full varlist
ST_retcode sf_ll_read_varlist(
    const char *fname,
    const int verbose,
    const int debug,
    const uint64_t strbuffer)
{
    ST_double z;
    ST_retcode rc = 0, any_rc = 0;
    int64_t nrow_groups, nrow, ncol, r, i, j, jsel;
    int64_t maxstrlen, values_read, rows_read, ix, ig;

    SPARQUET_CHAR (vmatrix, 32);
    SPARQUET_CHAR (vscalar, 32);

    // Declare all the value types
    // ---------------------------

    bool vbool;
    int32_t vint32;
    int64_t vint64;
    float vfloat;
    double vdouble;
    parquet::ByteArray vbytearray;
    parquet::FixedLenByteArray vfixedlen;

    // Declare all the readers
    // -----------------------

    std::shared_ptr<parquet::ColumnReader> column_reader;
    std::shared_ptr<parquet::RowGroupReader> row_group_reader;

    parquet::BoolReader* bool_reader;
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::FloatReader* float_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* ba_reader;
    parquet::FixedLenByteArrayReader* flba_reader;

    // Not implemented in Stata
    // ------------------------

    // parquet::Int96 vint96;
    // parquet::Int96Reader* int96_reader;

    // File reader
    // -----------

    const parquet::ColumnDescriptor* descr;
    try {
        std::unique_ptr<parquet::ParquetFileReader> parquet_reader =
            parquet::ParquetFileReader::OpenFile(fname, false);

        std::shared_ptr<parquet::FileMetaData> file_metadata =
            parquet_reader->metadata();

        // ncol = file_metadata->num_columns();
        nrow = file_metadata->num_rows();
        nrow_groups = file_metadata->num_row_groups();
        ix = 0;
        ig = 0;

        memcpy(vscalar, "__sparquet_ncol", 15);
        if ( (rc = SF_scal_use(vscalar, &z)) ) {
            any_rc = rc;
            ncol = 1;
        }
        else {
            ncol = (int64_t) z;
        }

        maxstrlen = 1;
        int64_t vtypes[ncol];
        int64_t colix[ncol];

        // Adjust column selection to be 0-indexed
        memset(vmatrix, '\0', 32);
        memcpy(vmatrix, "__sparquet_colix", 16);
        for (j = 0; j < ncol; j++) {
            if ( (rc = SF_mat_el(vmatrix, 1, j + 1, &z)) ) goto exit;
            colix[j] = (int64_t) z - 1;
        }

        // Encoded variable types
        memset(vmatrix, '\0', 32);
        memcpy(vmatrix, "__sparquet_coltypes", 19);
        for (j = 0; j < ncol; j++) {
            if ( (rc = SF_mat_el(vmatrix, 1, j + 1, &z)) ) goto exit;
            vtypes[j] = (int64_t) z;
            if ( vtypes[j] > maxstrlen ) maxstrlen = vtypes[j];
        }
        SPARQUET_CHAR (vstr, maxstrlen);

        if ( any_rc ) {
            rc = any_rc;
            goto exit;
        }

        sf_printf_debug(verbose, "\tFile:    %s\n",  fname);
        sf_printf_debug(verbose, "\tGroups:  %ld\n", nrow_groups);
        sf_printf_debug(verbose, "\tColumns: %ld\n", ncol);
        sf_printf_debug(verbose, "\tRows:    %ld\n", nrow);

        // Loop through each group
        // For each group, loop through each column
        // For each column, loop through each row
        // TODO: Missing values

        clock_t timer = clock();
        for (r = 0; r < nrow_groups; ++r) {
            values_read = 0;
            rows_read   = 0;
            ix += ig;
            row_group_reader = parquet_reader->RowGroup(r);
            for (j = 0; j < ncol; j++) {
                i = 0;
                jsel = colix[j];
                column_reader = row_group_reader->Column(jsel);
                descr = file_metadata->schema()->Column(jsel);
                switch (descr->physical_type()) {
                    case Type::BOOLEAN:    // byte
                        bool_reader = static_cast<parquet::BoolReader*>(column_reader.get());
                        while (bool_reader->HasNext()) {
                            rows_read = bool_reader->ReadBatch(1, nullptr, nullptr, &vbool, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            // sf_printf_debug(debug, "\t(bool, %ld, %ld): %9.4f\n", j, i + ix, (ST_double) vbool);
                            if ( (rc = SF_vstore(j + 1, i + ix, (ST_double) vbool)) ) goto exit;
                        }
                        break;
                    case Type::INT32:      // long
                        int32_reader = static_cast<parquet::Int32Reader*>(column_reader.get());
                        while (int32_reader->HasNext()) {
                            rows_read = int32_reader->ReadBatch(1, nullptr, nullptr, &vint32, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            // sf_printf_debug(debug, "\t(int32, %ld, %ld): %9.4f\n", j, i + ix, (ST_double) vint32);
                            if ( (rc = SF_vstore(j + 1, i + ix, (ST_double) vint32)) ) goto exit;
                        }
                        break;
                    case Type::INT64:      // double
                        int64_reader = static_cast<parquet::Int64Reader*>(column_reader.get());
                        while (int64_reader->HasNext()) {
                            rows_read = int64_reader->ReadBatch(1, nullptr, nullptr, &vint64, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            // sf_printf_debug(debug, "\t(int64, %ld, %ld): %9.4f\n", j, i + ix, (ST_double) vint64);
                            if ( (rc = SF_vstore(j + 1, i + ix, (ST_double) vint64)) ) goto exit;
                        }
                        break;
                    case Type::INT96:
                        sf_errprintf("96-bit integers not implemented.\n");
                        rc = 17101;
                        goto exit;
                    case Type::FLOAT:      // float
                        float_reader = static_cast<parquet::FloatReader*>(column_reader.get());
                        while (float_reader->HasNext()) {
                            rows_read = float_reader->ReadBatch(1, nullptr, nullptr, &vfloat, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            // sf_printf_debug(debug, "\t(float, %ld, %ld): %9.4f\n", j, i + ix, (ST_double) vfloat);
                            if ( (rc = SF_vstore(j + 1, i + ix, vfloat)) ) goto exit;
                        }
                        break;
                    case Type::DOUBLE:     // double
                        double_reader = static_cast<parquet::DoubleReader*>(column_reader.get());
                        while (double_reader->HasNext()) {
                            rows_read = double_reader->ReadBatch(1, nullptr, nullptr, &vdouble, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            // sf_printf_debug(debug, "\t(double, %ld, %ld): %9.4f\n", j, i + ix, (ST_double) vdouble);
                            if ( (rc = SF_vstore(j + 1, i + ix, vdouble)) ) goto exit;
                        }
                        break;
                    case Type::BYTE_ARRAY: // str#, strL
                        ba_reader = static_cast<parquet::ByteArrayReader*>(column_reader.get());
                        while (ba_reader->HasNext()) {
                            rows_read = ba_reader->ReadBatch(1, nullptr, nullptr, &vbytearray, &values_read);
                            assert(rows_read == 1);
                            i++;
                            if ( vbytearray.len > vtypes[j] ) {
                                sf_errprintf("Buffer (%d) too small; re-run with larger buffer or -strscan(.)-\n", vtypes[j]);
                                sf_errprintf("Group %d, row %d, col %d had a string of length %d.\n", r, i + ix, j, vbytearray.len);
                                rc = 17103;
                                goto exit;
                            }
                            memcpy(vstr, vbytearray.ptr, vbytearray.len);
                            if ( (rc = SF_sstore(j + 1, i + ix, vstr)) ) goto exit;
                            // sf_printf_debug(debug, "\t(BA, %ld, %ld): %s\n", j, i + ix, vstr);
                            memset(vstr, '\0', maxstrlen);
                        }
                        break;
                    case Type::FIXED_LEN_BYTE_ARRAY:
                        if ( descr->type_length() > vtypes[j] ) {
                            sf_errprintf("Buffer (%d) too small; error parsing FixedLenByteArray.\n", vtypes[j]);
                            sf_errprintf("Group %d, row %d, col %d had a string of length %d.\n", r, i + ix, j, descr->type_length());
                            rc = 17103;
                            goto exit;
                        }
                        flba_reader = static_cast<parquet::FixedLenByteArrayReader*>(column_reader.get());
                        while (flba_reader->HasNext()) {
                            rows_read = flba_reader->ReadBatch(1, nullptr, nullptr, &vfixedlen, &values_read);
                            assert(rows_read == 1);
                            assert(values_read == 1);
                            i++;
                            memcpy(vstr, vfixedlen.ptr, descr->type_length());
                            if ( (rc = SF_sstore(j + 1, i + ix, vstr)) ) goto exit;
                            memset(vstr, '\0', maxstrlen);
                        }
                    default:
                        sf_errprintf("Unknown parquet type.\n");
                        rc = 17100;
                        goto exit;
                }
                ig = i > ig? i: ig;
            }
        }
        sf_running_timer (&timer, "Read data from disk");

    } catch (const std::exception& e) {
        sf_errprintf("Parquet read error: %s\n", e.what());
        return(-1);
    }

exit:
    return(rc);
}
