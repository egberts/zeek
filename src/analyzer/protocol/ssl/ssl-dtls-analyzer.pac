
%extern{
#include <vector>
#include <algorithm>
#include <iostream>
#include <iterator>

#include "zeek/util.h"
#include "zeek/file_analysis/Manager.h"
%}

refine connection SSL_Conn += {

	%member{
		int established_;
	%}

	%init{
		established_ = false;
	%}

	%cleanup{
	%}

	function setEstablished() : bool
		%{
		established_ = true;
		return true;
		%}

	function proc_alert(rec: SSLRecord, level : int, desc : int) : bool
		%{
		if ( ssl_alert )
			zeek::BifEvent::enqueue_ssl_alert(zeek_analyzer(), zeek_analyzer()->Conn(),
							${rec.is_orig}, level, desc);
		return true;
		%}
	function proc_unknown_record(rec: SSLRecord) : bool
		%{
		zeek_analyzer()->AnalyzerViolation(zeek::util::fmt("unknown SSL record type (%d) from %s",
				${rec.content_type},
				orig_label(${rec.is_orig}).c_str()));
		return true;
		%}

	function proc_ciphertext_record(rec : SSLRecord) : bool
		%{
		if ( established_ == false && determine_tls13() == 1 )
			{
			if ( ssl_probable_encrypted_handshake_message )
				zeek::BifEvent::enqueue_ssl_probable_encrypted_handshake_message(zeek_analyzer(),
					zeek_analyzer()->Conn(), ${rec.is_orig}, ${rec.length});
			}

		if ( client_state_ == STATE_ENCRYPTED &&
		     server_state_ == STATE_ENCRYPTED &&
		     established_ == false )
			{
			established_ = true;
			if ( ssl_established )
				zeek::BifEvent::enqueue_ssl_established(zeek_analyzer(), zeek_analyzer()->Conn());
			}

		if ( ssl_encrypted_data )
			zeek::BifEvent::enqueue_ssl_encrypted_data(zeek_analyzer(),
				zeek_analyzer()->Conn(), ${rec.is_orig}, ${rec.raw_tls_version}, ${rec.content_type}, ${rec.length});

		return true;
		%}

	function proc_plaintext_record(rec : SSLRecord) : bool
		%{
		if ( ssl_plaintext_data )
			zeek::BifEvent::enqueue_ssl_plaintext_data(zeek_analyzer(),
				zeek_analyzer()->Conn(), ${rec.is_orig}, ${rec.raw_tls_version}, ${rec.content_type}, ${rec.length});

		return true;
		%}

	function proc_heartbeat(rec : SSLRecord, type: uint8, payload_length: uint16, data: bytestring) : bool
		%{
		if ( ssl_heartbeat )
			zeek::BifEvent::enqueue_ssl_heartbeat(zeek_analyzer(),
				zeek_analyzer()->Conn(), ${rec.is_orig}, ${rec.length}, type, payload_length,
				zeek::make_intrusive<zeek::StringVal>(data.length(), (const char*) data.data()));
		return true;
		%}

	function proc_check_v2_server_hello_version(version: uint16) : bool
		%{
		if ( version != SSLv20 )
			{
			zeek_analyzer()->AnalyzerViolation(zeek::util::fmt("Invalid version in SSL server hello. Version: %d", version));
			zeek_analyzer()->SetSkip(true);
			return false;
			}

		return true;
		%}


	function proc_ccs(rec: SSLRecord) : bool
		%{
		if ( ssl_change_cipher_spec )
			zeek::BifEvent::enqueue_ssl_change_cipher_spec(zeek_analyzer(),
				zeek_analyzer()->Conn(), ${rec.is_orig});

		return true;
		%}

};

refine typeattr Alert += &let {
	proc : bool = $context.connection.proc_alert(rec, level, description);
};

refine typeattr Heartbeat += &let {
	proc : bool = $context.connection.proc_heartbeat(rec, type, payload_length, data);
};

refine typeattr UnknownRecord += &let {
	proc : bool = $context.connection.proc_unknown_record(rec);
};

refine typeattr CiphertextRecord += &let {
	proc : bool = $context.connection.proc_ciphertext_record(rec);
}

refine typeattr PlaintextRecord += &let {
	proc : bool = $context.connection.proc_plaintext_record(rec);
}

refine typeattr ChangeCipherSpec += &let {
	proc : bool = $context.connection.proc_ccs(rec);
};
