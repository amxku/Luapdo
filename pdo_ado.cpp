#include <stdio.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif 

#include "pdo_internal.h"

#ifdef __cplusplus
}
#endif 


#import "c:\Program Files\Common Files\System\ADO\msado15.dll" no_namespace rename("EOF","adoEOF")


extern "C" void * pdo_ado_native(pdo_handle_t *handle)
{
	return (void *)handle;
}

extern "C" pdo_handle_t *pdo_ado_open(const char *params, int * errnumber, const char * error, size_t errlen)
{
	HRESULT	result = -1;

	_ConnectionPtr *conn = new _ConnectionPtr;
	try	{
		HRESULT hr = conn->CreateInstance (__uuidof (Connection));
		if (SUCCEEDED (hr)) {
			result = (*conn)->Open (_bstr_t (params), _bstr_t (""), _bstr_t (""), adModeUnknown);
		}
	} catch ( _com_error &e ) {
		if (errnumber) {
			*errnumber = e.WCode();
		}
		strncpy((char *)error, (LPCTSTR)e.Description(), errlen);
	} catch (...) {
		strncpy((char *)error, "Unhandled Exception", errlen);
	}

	if (FAILED(result)) {
		conn->Release();
		delete conn;
		conn = NULL;
	}

	return (pdo_handle_t *)conn;
}

extern "C" pdo_status_t pdo_ado_check_conn(pdo_handle_t *handle)
{
	_ConnectionPtr *conn = (_ConnectionPtr *)handle;
	return ((*conn)->State != adStateClosed) ? PDO_SUCCESS : PDO_EGENERAL;
}


extern "C" pdo_status_t pdo_ado_close(pdo_handle_t *handle)
{
	_ConnectionPtr *conn = (_ConnectionPtr *)handle;
	if ((*conn)->State != adStateClosed) {
		(*conn)->Close();   
	}
	conn->Release();

	delete conn;
	conn = NULL;
	return PDO_SUCCESS;
}

extern "C" pdo_status_t pdo_ado_select_db(pdo_handle_t *handle, const char *name)
{
	return PDO_ENOTEMPL;
}

extern "C" int pdo_ado_query(pdo_handle_t *handle,                                  /* An open database */
	int *nrows, 
	const char *statement,                           /* SQL to be evaluated */
	pdo_callback callback,  /* Callback function */
	void *arg)
{
	int	ret = PDO_SUCCESS;
	_ConnectionPtr *conn = (_ConnectionPtr *)handle;

	try {
		if (!callback) {
			(*conn)->Execute(statement, NULL, adCmdText);
		}
		else
		{
			HRESULT	hr;
			_CommandPtr pCommand(__uuidof(Command));
			_RecordsetPtr pRecordset(__uuidof(Recordset));

			pCommand->ActiveConnection = *conn;
			pCommand->CommandText = statement;

			pRecordset->Open((IDispatch *) pCommand, vtMissing, adOpenForwardOnly,
							adLockReadOnly, adCmdUnknown);

			Fields* fields = NULL;
			long num_columns, i;
			hr = pRecordset->get_Fields(&fields);
			if (SUCCEEDED(hr)) {
				fields->get_Count(&num_columns);
				
				char **names = NULL;
				char **values = NULL;
				BOOL is_break = FALSE;

				// 申请两个数组，方便回调
				names = (char **)malloc(sizeof(char *) * num_columns);
				values = (char **)malloc(sizeof(char *) * num_columns);

				for (i = 0; i < num_columns; i++) {
					BSTR bstrColName;
					fields->Item[i]->get_Name(&bstrColName);
					names[i] = _com_util::ConvertBSTRToString(bstrColName);
				}
				// 释放
				fields->Release();
				fields = NULL;

				// 读出命令结果
				while (!pRecordset->adoEOF && !is_break) {
					for(i = 0; i < num_columns; i++) {
						_variant_t var = pRecordset->GetCollect(i);
						if (var.vt != VT_NULL) {
							values[i] = strdup((LPSTR)_bstr_t(var));
						} else {
							values[i] = NULL;
						}
					}

					/* nozero to break */
					is_break = (BOOL)callback(arg, num_columns, values, names);

					// 释放值
					for (i = 0; i < num_columns; i++) {	
						if (values[i]) {
							free(values[i]);
						}
					}

					pRecordset->MoveNext();
				}

				// 释放列名
				for (i = 0; i < num_columns; i++) {	
					if (names[i]) {
						free(names[i]);
					}
				}

				// 释放数组
				free(names);
				free(values);
			}			

			pRecordset->Close();
		}
	} catch( _com_error &e ) {
		ret = e.Error();
	} catch (...) {
		ret = -1;
	}
	return ret;
}

extern "C" int pdo_ado_errno(pdo_handle_t *handle)
{
	int	ret = 0;
	_ConnectionPtr *conn = (_ConnectionPtr *)handle;
	long	err_count = (*conn)->Errors->Count;
	if (err_count)	{
		ErrorPtr err;
		err = (*conn)->Errors->GetItem((long)(err_count-1));
		ret = err->GetNativeError();
	}
	
	return ret;
}

extern "C" pdo_status_t pdo_ado_error(pdo_handle_t *handle, const char * error, size_t errlen)
{
	_ConnectionPtr *conn = (_ConnectionPtr *)handle;
	long	err_count = (*conn)->Errors->Count;
	if (err_count) {
		ErrorPtr err;
		err = (*conn)->Errors->GetItem((long)(err_count-1));
		strncpy((char *)error, err->GetDescription(), errlen);
	} else {
		strncpy((char *)error, "No error", errlen);
	}

	return PDO_SUCCESS;
}
extern "C" const char *pdo_ado_escape(pdo_handle_t *handle, const char *string)
{
	return strdup(string);
}

extern "C" const struct pdo_driver_t pdo_ado_driver = {
	"ado",
	pdo_ado_native,
	pdo_ado_open,
	pdo_ado_check_conn,
	pdo_ado_close,
	pdo_ado_select_db,
	pdo_ado_query,
	pdo_ado_errno,
	pdo_ado_error,
	pdo_ado_escape,
};