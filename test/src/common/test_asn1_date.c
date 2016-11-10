/* ##__FLEA_LICENSE_TEXT_PLACEHOLDER__## */

#include "internal/common/default.h"
#include "flea/asn1_date.h"
#include "flea/array_util.h"
#include "flea/error_handling.h"
#include "flea/alloc.h"

flea_u8_t flea_test_asn1_gmttime_strlen(flea_u8_t* in)
{
	flea_u8_t *p = in;	
	flea_u8_t i = 0;
	while (p[i] != 0x00)
	{
		i++;
	}
	
	return i;
}

flea_err_t THR_flea_test_asn1_date()
{
	
	FLEA_THR_BEG_FUNC();

	flea_u8_t input[] = "910506164540Z";	
	flea_dtl_t length = sizeof(input)-1;
	
	flea_gmt_time_t output;

	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, input, length, &output));

	if (output.year != 1991 || output.month != 5 || output.day != 6 || output.hours != 16 || output.minutes != 45 || output.seconds != 40)
	{
		FLEA_THROW("parse ASN.1 utc time incorrect", FLEA_ERR_FAILED_TEST);
	}

	flea_u8_t input2[] = "19910506164540Z";	
	flea_dtl_t length2 = sizeof(input2)-1;
	
	flea_gmt_time_t output2;

	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_generalized_time, input2, length2, &output2));

	if (output2.year != 1991 || output2.month != 5 || output2.day != 6 || output2.hours != 16 || output2.minutes != 45 || output2.seconds != 40)
	{
		FLEA_THROW("parse ASN.1 generalized time incorrect", FLEA_ERR_FAILED_TEST);
	}


	/*	
	unsigned char date1[] = { 0x39, 0x31, 0x30, 0x35, 0x30, 0x36, 0x31, 0x36, 0x34, 0x35, 0x34, 0x30, 0x2d, 0x30, 0x37, 0x30, 0x30 };
	unsigned char date2[] = { 0x39, 0x32, 0x30, 0x35, 0x30, 0x36, 0x31, 0x36, 0x34, 0x35, 0x34, 0x30, 0x2d, 0x30, 0x37, 0x30, 0x30 };
	unsigned char date3[] = { 0x39, 0x33, 0x30, 0x35, 0x30, 0x36, 0x31, 0x36, 0x34, 0x35, 0x34, 0x30, 0x2d, 0x30, 0x37, 0x35, 0x30 };
	unsigned char date4[] = { 0x39, 0x33, 0x30, 0x35, 0x30, 0x36, 0x31, 0x36, 0x34, 0x35, 0x34, 0x30, 0x2d, 0x30, 0x37, 0x30, 0x30 };

	// must correctly calculate leap years and offsets for the following two dates to be ordered correctly
	unsigned char date5[] = { 0x31, 0x36, 0x30, 0x32, 0x32, 0x39, 0x32, 0x32, 0x30, 0x30, 0x2d, 0x30, 0x37, 0x30, 0x30 };	// 29.02.16 22:00 - 07:00
	unsigned char date6[] = { 0x31, 0x36, 0x30, 0x33, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x5A };							// 01.03.16 00:00

	// test century borders at YY=49 and YY=50
	unsigned char date7[] = { 0x34, 0x39, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x5A };	// 01.01.2049 00:00
	unsigned char date8[] = { 0x35, 0x30, 0x31, 0x32, 0x33, 0x31, 0x32, 0x32, 0x30, 0x30, 0x5A };	// 31.12.1950 22:00

	unsigned char date9[] =  { 0x34, 0x39, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x5A };							// 01.01.2049 00:00
	unsigned char date10[] = { 0x35, 0x30, 0x31, 0x32, 0x33, 0x31, 0x32, 0x32, 0x30, 0x30, 0x2d, 0x30, 0x33, 0x30, 0x30 };	// 31.12.1950 22:00 - 03:00

	flea_utc_time_t d1, d2, d3, d4, d5, d6, d7, d8, d9, d10;
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date1, sizeof(date1), &d1));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date2, sizeof(date2), &d2));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date3, sizeof(date3), &d3));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date4, sizeof(date4), &d4));

	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date5, sizeof(date5), &d5));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date6, sizeof(date6), &d6));
	
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date7, sizeof(date7), &d7));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date8, sizeof(date8), &d8));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date9, sizeof(date9), &d9));
	FLEA_CCALL(THR_flea_asn1_parse_date(flea_asn1_utc_time, date10, sizeof(date10), &d10));

	flea_s8_t result1, result2, result3, result4, result5, result6, result7, result8, result9, result10, result11;
	result1 = flea_asn1_cmp_utc_time(&d1, &d2);
	result2 = flea_asn1_cmp_utc_time(&d2, &d3);
	result3 = flea_asn1_cmp_utc_time(&d3, &d4);
	result4 = flea_asn1_cmp_utc_time(&d2, &d4);
	result5 = flea_asn1_cmp_utc_time(&d1, &d1);
	result6 = flea_asn1_cmp_utc_time(&d2, &d1);

	result7 = flea_asn1_cmp_utc_time(&d5, &d6);

	result8  = flea_asn1_cmp_utc_time(&d7, &d8);
	result9  = flea_asn1_cmp_utc_time(&d8, &d7);
	result10 = flea_asn1_cmp_utc_time(&d9, &d10);
	result11 = flea_asn1_cmp_utc_time(&d10, &d9);
	
	if (result1 != -1 || result2 != -1 || result3 != 1 || result4 != -1 || result5 != 0 || result6 != 1)
	{
		FLEA_THROW("comparison of utc_time failed", FLEA_ERR_FAILED_TEST);
	}
	
	if (result7 != 1 || result8 != 1 || result9 != -1 || result10 != 1 || result11 != -1)
	{
		FLEA_THROW("comparison of utc_time failed (2)", FLEA_ERR_FAILED_TEST);
	}

	if (  	flea_asn1_cmp_utc_time(&d1, &d2) != -flea_asn1_cmp_utc_time(&d2, &d1)
		||	flea_asn1_cmp_utc_time(&d2, &d3) != -flea_asn1_cmp_utc_time(&d3, &d2)
		||	flea_asn1_cmp_utc_time(&d3, &d4) != -flea_asn1_cmp_utc_time(&d4, &d3)
		||	flea_asn1_cmp_utc_time(&d4, &d5) != -flea_asn1_cmp_utc_time(&d5, &d4)
		||	flea_asn1_cmp_utc_time(&d5, &d6) != -flea_asn1_cmp_utc_time(&d6, &d5)
		||	flea_asn1_cmp_utc_time(&d6, &d7) != -flea_asn1_cmp_utc_time(&d7, &d6)
		||	flea_asn1_cmp_utc_time(&d7, &d8) != -flea_asn1_cmp_utc_time(&d8, &d7)
		||	flea_asn1_cmp_utc_time(&d8, &d9) != -flea_asn1_cmp_utc_time(&d9, &d8)
		||	flea_asn1_cmp_utc_time(&d9, &d10) != -flea_asn1_cmp_utc_time(&d10, &d9)
		||	flea_asn1_cmp_utc_time(&d10, &d6) != -flea_asn1_cmp_utc_time(&d6, &d10)
		||	flea_asn1_cmp_utc_time(&d3, &d7) != -flea_asn1_cmp_utc_time(&d7, &d3)
		||	flea_asn1_cmp_utc_time(&d2, &d9) != -flea_asn1_cmp_utc_time(&d9, &d2)
		)
	{
		FLEA_THROW("comparison of utc_time failed (3)", FLEA_ERR_FAILED_TEST);
	}

	if (	flea_asn1_cmp_utc_time(&d1, &d1) != 0
		|| 	flea_asn1_cmp_utc_time(&d2, &d2) != 0
		|| 	flea_asn1_cmp_utc_time(&d3, &d3) != 0
		|| 	flea_asn1_cmp_utc_time(&d4, &d4) != 0
		|| 	flea_asn1_cmp_utc_time(&d5, &d5) != 0
		|| 	flea_asn1_cmp_utc_time(&d6, &d6) != 0
		|| 	flea_asn1_cmp_utc_time(&d7, &d7) != 0
		|| 	flea_asn1_cmp_utc_time(&d8, &d8) != 0
		|| 	flea_asn1_cmp_utc_time(&d9, &d9) != 0
		|| 	flea_asn1_cmp_utc_time(&d10, &d10) != 0
		)
	{
		FLEA_THROW("comparison of utc_time failed (4)", FLEA_ERR_FAILED_TEST);
	}


	
		list based tests
	
		{ "<date a>", "<date b>", exp_res },
		{ "<date a>", "<date b>", exp_res },
		...
	*/
	
	typedef struct
	{
	  flea_u8_t* date_a;
	  flea_u8_t* date_b;
	  flea_s8_t  result;
	} flea_asn1_date_test_case;
	
	enum flea_utc_time_t_test_result { FLEA_UTCTIME_DEC_ERROR=2, FLEA_UTCTIME_GREATER=1, FLEA_UTCTIME_EQUAL=0, FLEA_UTCTIME_LESS=-1  };
	enum flea_generalized_time_t_test_result { FLEA_GENERALIZEDTIME_DEC_ERROR=2, FLEA_GENERALIZEDTIME_GREATER=1, FLEA_GENERALIEZDTIME_EQUAL=0, FLEA_GENERALIZEDTIME_LESS=-1  };

	flea_asn1_date_test_case utc_test_cases[] = { 
												// old tests with offset, should all throw errors
												(flea_asn1_date_test_case){(flea_u8_t*)"910506164540-0700", (flea_u8_t*)"910506164540-0700", FLEA_UTCTIME_DEC_ERROR },
							 					(flea_asn1_date_test_case){(flea_u8_t*)"920506164540-0700", (flea_u8_t*)"910506164540-0700", FLEA_UTCTIME_DEC_ERROR },

												(flea_asn1_date_test_case){(flea_u8_t*)"160228200000-0000", (flea_u8_t*)"1602282000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"160228200000-0100", (flea_u8_t*)"1602282100Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"160228200000-0400", (flea_u8_t*)"1602290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"170228200000-5500", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"abq228200000-0000", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"17022820000-5500", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"170228200000-00", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"170228200000-", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"170228200000/0000", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"1", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"22", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"223", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"ABC170228200000-0000", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												(flea_asn1_date_test_case){(flea_u8_t*)"000000000000000-i500", (flea_u8_t*)"1702290000Z", FLEA_UTCTIME_DEC_ERROR },
												
												(flea_asn1_date_test_case){(flea_u8_t*)"490228200000Z", (flea_u8_t*)"500228200000Z", FLEA_UTCTIME_GREATER }

											};

	flea_asn1_date_test_case gen_test_cases[] = { 						
												(flea_asn1_date_test_case){(flea_u8_t*)"19490228200000Z", (flea_u8_t*)"19500228200000Z", FLEA_GENERALIZEDTIME_LESS }

											};


	flea_gmt_time_t date_a__t;
	flea_gmt_time_t date_b__t;
	flea_s8_t res__t;
	

	flea_err_t res_1__t, res_2__t;

	for (flea_u8_t i=0; i < (sizeof(utc_test_cases) / sizeof(flea_asn1_date_test_case)); i++) 
	{
		// parse dates
		res_1__t = THR_flea_asn1_parse_utc_time(utc_test_cases[i].date_a, flea_test_asn1_gmttime_strlen(utc_test_cases[i].date_a), &date_a__t);
		res_2__t = THR_flea_asn1_parse_utc_time(utc_test_cases[i].date_b, flea_test_asn1_gmttime_strlen(utc_test_cases[i].date_b), &date_b__t);
		
		if (res_1__t != FLEA_ERR_FINE)
		{
			if (res_1__t == FLEA_ERR_ASN1_DER_DEC_ERR && utc_test_cases[i].result == FLEA_UTCTIME_DEC_ERROR)
				continue;
			else
				FLEA_THROW("Decode error in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}
		if (res_2__t != FLEA_ERR_FINE)
		{
			if (res_2__t == FLEA_ERR_ASN1_DER_DEC_ERR && utc_test_cases[i].result == FLEA_UTCTIME_DEC_ERROR)
				continue;
			else
				FLEA_THROW("Decode error in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}

		// compare dates
		res__t = flea_asn1_cmp_utc_time(&date_a__t, &date_b__t);
		
		// check result
		if (res__t != utc_test_cases[i].result)
		{
			FLEA_THROW("Unexpected result in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}
	}


	for (flea_u8_t i=0; i < (sizeof(gen_test_cases) / sizeof(flea_asn1_date_test_case)); i++) 
	{
		// parse dates
		res_1__t = THR_flea_asn1_parse_generalized_time(gen_test_cases[i].date_a, flea_test_asn1_gmttime_strlen(gen_test_cases[i].date_a), &date_a__t);
		res_2__t = THR_flea_asn1_parse_generalized_time(gen_test_cases[i].date_b, flea_test_asn1_gmttime_strlen(gen_test_cases[i].date_b), &date_b__t);
		
		if (res_1__t != FLEA_ERR_FINE)
		{
			if (res_1__t == FLEA_ERR_ASN1_DER_DEC_ERR && gen_test_cases[i].result == FLEA_UTCTIME_DEC_ERROR)
				continue;
			else
				FLEA_THROW("Decode error in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}
		if (res_2__t != FLEA_ERR_FINE)
		{
			if (res_2__t == FLEA_ERR_ASN1_DER_DEC_ERR && gen_test_cases[i].result == FLEA_UTCTIME_DEC_ERROR)
				continue;
			else
				FLEA_THROW("Decode error in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}

		// compare dates
		res__t = flea_asn1_cmp_utc_time(&date_a__t, &date_b__t);
		
		// check result
		if (res__t != gen_test_cases[i].result)
		{
			FLEA_THROW("Unexpected result in list based tests for flea_asn1_cmp_utc_time()", FLEA_ERR_FAILED_TEST);
		}
	}
	

	FLEA_THR_FIN_SEC();
}

